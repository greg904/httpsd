// For accept4
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "parser.h"

#ifndef SERVER_PORT
#define SERVER_PORT 8080
#endif

#define MAX_CLIENTS 16

// Represents a socket connection to the server socket. Note that the size of
// this struct is precisely 256 bytes.
struct client {
	// If we haven't received a valid request after the timeout, we will
	// close the socket and free the client object.
	// This is to prevent potential badly behaving clients that would open
	// a connection to the server, not send anything (or not finish the
	// request) and never close the connection from taking up space and
	// preventing other good clients from connecting.
	uint64_t timeout;

	int socket_fd;

	// The write syscall might not write the whole buffer but only a part of
	// it, therefore we must keep track of how many bytes we have already
	// sent in order to know what to send the next time we get a EPOLLIN
	// event.
	uint16_t res_bytes_sent : 12;

	uint8_t req_parser_state : 4;

	// At the end of the parsing, this will contain the request URI, then
	// a NULL character, then the request host, then a NULL character or
	// no character if it's the end of the array.
	char req_fields[242];
};

enum _client_respond {
	// There is more data to send, the method must be called again.
	_cr_continue,

	_cr_error,

	// We finished sending the response to the client. The socket can be
	// closed.
	_cr_finished,
};

static struct client clients[MAX_CLIENTS];

// For every clients object that is currently used, a bit is set in this bitmap.
static uint16_t clients_bitmap;

static int epoll_fd;
static int server_fd;

static char tmp_buf[512];

static int client_allocate();
static bool client_is_valid(int index);
static void client_free(int index);
static size_t client_write_response(struct client *c, char *response);
static enum _client_respond client_continue_sending_response(struct client *c);

static bool epoll_on_server_in();
static bool epoll_on_client_in(int client_index);
static bool epoll_on_client_out(int client_index);
static bool epoll_on_event();

int main(int argc, char **argv)
{
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1) {
		perror("epoll_create()");
		return EXIT_FAILURE;
	}

	server_fd =
	    socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (server_fd == -1) {
		perror("socket()");
		return 1;
	}

	struct sockaddr_in addr = {};
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind()");
		return 1;
	}

	struct epoll_event server_epoll_event;
	server_epoll_event.data.u64 = 0;
	// We want to be notified when the server socket is ready to accept a
	// client socket.
	server_epoll_event.events = EPOLLIN | EPOLLET | EPOLLWAKEUP;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd,
		      &server_epoll_event) == -1) {
		perror("epoll_ctl()");
		return 1;
	}

	if (listen(server_fd, 4) == -1) {
		perror("listen()");
		return 1;
	}

	for (;;) {
		struct epoll_event events[32];

		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			perror("clock_gettime()");
			return 1;
		}
		uint64_t now_msec = now.tv_sec * 1000 + now.tv_nsec / 1000000;

		uint16_t clients_to_timeout = 0;
		int epoll_timeout = -1;

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!client_is_valid(i))
				continue;

			struct client *c = &clients[i];
			if (now_msec >= c->timeout) {
				// The request has already timed out
				close(c->socket_fd);
				client_free(i);
				continue;
			}

			uint64_t diff_msec = c->timeout - now_msec;
			if (epoll_timeout == -1 || diff_msec < epoll_timeout) {
				clients_to_timeout = 1 << i;
				epoll_timeout = diff_msec;
			} else if (diff_msec == epoll_timeout) {
				clients_to_timeout |= 1 << i;
			}
		}

		int ret =
		    epoll_wait(epoll_fd, events,
			       sizeof(events) / sizeof(*events), epoll_timeout);
		if (ret == 0) {
			// The client had a chance to send us something before
			// the timeout but it didn't do so, so we will close the
			// socket and free the client object.
			assert(clients_to_timeout != 0);
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if ((clients_to_timeout & (1 << i)) != 0) {
					close(clients[i].socket_fd);
					client_free(i);
				}
			}
			continue;
		}
		if (ret == -1) {
			perror("epoll_wait()");
			return 1;
		}

		for (int i = 0; i < ret; i++) {
			if (!epoll_on_event(&events[i]))
				return 1;
		}
	}
}

static int client_allocate()
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		uint16_t mask = 1 << i;
		// Check if that index is already used
		if ((clients_bitmap & mask) == 0) {
			clients_bitmap |= mask;
			return i;
		}
	}
	return -1;
}

static bool client_is_valid(int index)
{
	uint16_t mask = 1 << index;
	return (clients_bitmap & mask) != 0;
}

static void client_free(int index)
{
	clients_bitmap &= ~(1 << index);

	// Reset the fields for later
	struct client *c = &clients[index];
	c->res_bytes_sent = 0;
	c->req_parser_state = ps_method_0;
	memset(c->req_fields, 0, sizeof(c->req_fields));
}

static size_t client_write_response(struct client *c, char *response)
{
	char *tmp = response;

	const char response_start[] =
	    "HTTP/1.1 301 Moved Permanently\r\nLocation: https://";
	memcpy(response, response_start, sizeof(response_start) - 1);
	tmp += sizeof(response_start) - 1;

	size_t sep_index = strlen(c->req_fields);

	char *host_start = c->req_fields + sep_index + 1;
	char *host_end = host_start;
	while (*host_end != '\0' && host_end != c->req_fields + sizeof(c->req_fields))
		host_end++;
	size_t host_len = host_end - host_start;

	// Host
	memcpy(tmp, c->req_fields + sep_index + 1, host_len);
	tmp += host_len;

	// Slash
	*tmp = '/';
	tmp++;

	// URI
	memcpy(tmp, c->req_fields, sep_index);
	tmp += sep_index;

	const char response_end[] = "\r\n\r\n";
	memcpy(tmp, response_end, sizeof(response_end) - 1);
	tmp += sizeof(response_end) - 1;

	return tmp - response;
}

static enum _client_respond client_continue_sending_response(struct client *c)
{
	size_t res_len = client_write_response(c, tmp_buf);

	for (;;) {
		size_t remaining = res_len - c->res_bytes_sent;
		if (remaining == 0)
			return _cr_finished;

		ssize_t written =
		    write(c->socket_fd, tmp_buf + c->res_bytes_sent, remaining);
		if (written == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return _cr_continue;

			perror("write()");
			return _cr_error;
		}
		c->res_bytes_sent += written;
	}
}

static bool epoll_on_server_in()
{
	// The server socket is ready to accept one or
	// many connection(s).

	for (;;) {
		int client_fd = accept4(server_fd, NULL, NULL,
					SOCK_CLOEXEC | SOCK_NONBLOCK);
		if (client_fd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// We have already accepted all connections.
				break;
			}

			perror("accept()");
			return false;
		}

		int client_index = client_allocate();
		if (client_index == -1) {
			// Too many clients right now, sorry!
			close(client_fd);
			continue;
		}
		struct client *c = &clients[client_index];

		c->socket_fd = client_fd;

		// Setup the timeout
		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			perror("clock_gettime()");
			return false;
		}
		c->timeout = now.tv_sec * 1000 + now.tv_nsec / 1000000 + 2000;

		struct epoll_event client_epoll_event;
		client_epoll_event.data.u64 = client_index + 1;
		// For now, we only care about reading the request. Later, when
		// we want to know when we can write to the socket to respond to
		// the request, we will call epoll_ctl to modify the events.
		client_epoll_event.events = EPOLLIN | EPOLLET | EPOLLWAKEUP;

		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd,
			      &client_epoll_event) == -1) {
			perror("epoll_ctl()");
			return false;
		}
	}

	return true;
}

static bool epoll_on_client_in(int client_index)
{
	struct client *c = &clients[client_index];

	bool continue_read = true;
	while (continue_read) {
		int bytes_read = read(c->socket_fd, tmp_buf, sizeof(tmp_buf));
		if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// We have already read everything.
				break;
			}

			perror("read()");
			return false;
		}
		if (bytes_read == 0) {
			// EOS before we finished parsing, so this is an invalid
			// request. We will close the client's socket and forget
			// about it.
			close(c->socket_fd);
			client_free(client_index);
			return true;
		}

		uint8_t state = c->req_parser_state;
		enum parser_result r =
		    parser_go(&state, c->req_fields, sizeof(c->req_fields),
			      tmp_buf, bytes_read);
		switch (r) {
		case pr_continue:
			c->req_parser_state = state;
			continue;
		case pr_error:
			continue_read = false;
			// The socket FD will be removed from the epoll when it
			// is closed.
			close(c->socket_fd);
			client_free(client_index);
			break;
		case pr_finished: {
			enum _client_respond result =
			    client_continue_sending_response(c);
			switch (result) {
			case _cr_continue: {
				struct epoll_event new_client_epoll;
				new_client_epoll.data.u64 = client_index + 1;
				// We need to wait until we can write to the
				// socket again.
				new_client_epoll.events =
				    EPOLLOUT | EPOLLET | EPOLLWAKEUP;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
					      c->socket_fd,
					      &new_client_epoll) == -1) {
					perror("epoll_ctl()");
					return false;
				}

				continue_read = false;
				break;
			}
			case _cr_error:
				return false;
			case _cr_finished:
				continue_read = false;
				// The socket FD will be removed from the epoll
				// when it is closed.
				close(c->socket_fd);
				client_free(client_index);
				break;
			}

			break;
		}
		}
	}

	return true;
}

static bool epoll_on_client_out(int client_index)
{
	struct client *c = &clients[client_index];

	enum _client_respond result = client_continue_sending_response(c);
	switch (result) {
	case _cr_continue:
		break;
	case _cr_error:
		return false;
	case _cr_finished:
		// The socket FD will be removed from the epoll when it is
		// closed.
		close(c->socket_fd);
		client_free(client_index);
		break;
	}

	return true;
}

bool epoll_on_event(const struct epoll_event *event)
{
	bool in = (event->events & EPOLLIN) != 0;
	bool out = (event->events & EPOLLOUT) != 0;
	bool rdhup = (event->events & EPOLLRDHUP) != 0;
	bool err = (event->events & EPOLLERR) != 0;

	if (event->data.u64 == 0) {
		assert(!rdhup && !err);
		assert(in && !out);

		if (!epoll_on_server_in())
			return 1;
	} else {
		int client_index = event->data.u64 - 1;

		if (rdhup || err) {
			// We haven't had the chance yet to
			// finish sending the response because
			// we are still receiving events for the
			// FD. However, the writing half is now
			// closed or an error has occured on the
			// socket, so we must end the
			// connection.
			close(clients[client_index].socket_fd);
			client_free(client_index);
			return true;
		}

		// This should never fail because we first
		// register for EPOLLIN and then we register for
		// just EPOLLOUT as soon as we want to send the
		// response, so we should never have both of
		// them at the same time.
		assert(in != out);

		if (in && !epoll_on_client_in(client_index))
			return false;
		if (out && !epoll_on_client_out(client_index))
			return false;
	}

	return true;
}
