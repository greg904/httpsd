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

#ifndef SERVER_PORT
#define SERVER_PORT 8080
#endif

#define MAX_CLIENTS 16

enum request_state {
	// Expects the GET method
	request_method,

	// Reads the request URI
	request_uri,

	// Ignores everything until it encounters a CR in which case it switches
	// to request_lf
	request_ignore_line,

	// Expected a LF and switches to request_header_name
	request_lf,

	// Reads the header's name. Switches either to request_header_host if
	// the header's name is "Host" or to request_ignore_line if it is not.
	request_header_name,

	// Reads the Host header's value and responds to the client
	request_header_host,
};

struct client {
	int socket_fd;

	// If we haven't received a valid request after the timeout, we will
	// close the socket and free the client object.
	// This is to prevent potential badly behaving clients that would open
	// a connection to the server, not send anything (or not finish the
	// request) and never close the connection from taking up space and
	// preventing other good clients from connecting.
	struct timespec timeout;

	enum request_state parser_state;

	// This is for remembering where we are in the HTTP method or in a HTTP
	// header name if it gets split between two calls to read().
	uint8_t parser_tmp;

	char request_uri_buf[256];
	uint32_t request_uri_len;

	char request_host_buf[256];
	uint32_t request_host_len;

	char response_buf[1024];
	uint32_t response_len;
	uint32_t bytes_sent;
};

struct client clients[MAX_CLIENTS];

// For every clients object that is currently used, a bit is set in this bitmap.
uint16_t clients_bitmap;

int client_allocate()
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

bool client_is_valid(int index)
{
	uint16_t mask = 1 << index;
	return (clients_bitmap & mask) != 0;
}

void client_free(int index) { clients_bitmap &= ~(1 << index); }

enum parser_result {
	// The parser needs more data to make a decision.
	parser_result_continue,

	parser_result_failed,

	// The parsing has finished. A response can be made.
	parser_result_can_respond,
};

enum parser_result client_continue_parsing(struct client *c, const char *data,
					   size_t len)
{
	size_t read_index = 0;

	do {
		switch (c->parser_state) {
		case request_method: {
			const char method_str[] = "GET ";

			for (;;) {
				if (data[read_index++] !=
				    method_str[c->parser_tmp++]) {
					// Invalid HTTP method
					return parser_result_failed;
				}

				if (c->parser_tmp == sizeof(method_str) - 1) {
					c->parser_state = request_uri;
					c->parser_tmp = 0;
					break;
				}

				if (read_index >= len)
					break;
			}

			break;
		}
		case request_uri:
			for (;;) {
				char ch = data[read_index++];
				if (ch == ' ') {
					c->parser_state = request_ignore_line;
					break;
				}

				if (c->request_uri_len >=
				    sizeof(c->request_uri_buf)) {
					// The request URI is too long
					return parser_result_failed;
				}

				c->request_uri_buf[c->request_uri_len] = ch;
				c->request_uri_len++;

				if (read_index >= len)
					break;
			}

			break;
		case request_ignore_line:
			for (;;) {
				char ch = data[read_index++];
				if (ch == '\r') {
					c->parser_state = request_lf;
					break;
				}

				if (read_index >= len)
					break;
			}

			break;
		case request_lf:
			if (data[read_index++] != '\n') {
				// Expected a LF, but got something else
				return parser_result_failed;
			}

			c->parser_state = request_header_name;

			break;
		case request_header_name: {
			const char host_str[] = "Host: ";

			for (;;) {
				if (data[read_index++] !=
				    host_str[c->parser_tmp++]) {
					c->parser_state = request_ignore_line;
					c->parser_tmp = 0;
					break;
				}

				if (c->parser_tmp == sizeof(host_str) - 1) {
					c->parser_state = request_header_host;
					c->parser_tmp = 0;
					break;
				}

				if (read_index >= len)
					break;
			}

			break;
		}
		case request_header_host:
			for (;;) {
				char ch = data[read_index++];
				if (ch == '\r')
					return parser_result_can_respond;

				if (c->request_host_len >=
				    sizeof(c->request_host_buf)) {
					// The request host is too long
					return false;
				}

				c->request_host_buf[c->request_host_len] = ch;
				c->request_host_len++;

				if (read_index >= len)
					break;
			}

			break;
		}
	} while (read_index < len);

	// Everything parsed successfully.
	return parser_result_continue;
}

void memcpy_increment_dest(char *restrict *dest, const char *restrict src,
			   size_t n)
{
	memcpy(*dest, src, n);
	*dest += n;
}

void client_prepare_response(struct client *c)
{
	const char response_start[] =
	    "HTTP/1.1 301 Moved Permanently\r\nLocation: https://";
	const char response_end[] = "\r\n\r\n";

	size_t response_len = (sizeof(response_start) - 1) +
			      c->request_host_len + c->request_uri_len +
			      (sizeof(response_end) - 1);
	assert(response_len < sizeof(c->response_buf));

	char *tmp = c->response_buf;
	memcpy_increment_dest(&tmp, response_start, sizeof(response_start) - 1);
	memcpy_increment_dest(&tmp, c->request_host_buf, c->request_host_len);
	memcpy_increment_dest(&tmp, c->request_uri_buf, c->request_uri_len);
	memcpy_increment_dest(&tmp, response_end, sizeof(response_end) - 1);

	c->response_len = response_len;
}

enum client_respond {
	// There is more data to send, the method must be called again.
	client_respond_continue,

	client_respond_failed,

	// We finished sending the response to the client. The socket can be
	// closed.
	client_respond_finished,
};

enum client_respond client_continue_sending_response(struct client *c)
{
	for (;;) {
		size_t remaining = c->response_len - c->bytes_sent;
		if (remaining == 0)
			return client_respond_finished;

		ssize_t written = write(
		    c->socket_fd, c->response_buf + c->bytes_sent, remaining);
		if (written == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return client_respond_continue;

			perror("write()");
			return client_respond_failed;
		}
		c->bytes_sent += written;
	}
}

int epoll_fd;
int server_fd;

bool epoll_on_server_in()
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

		// Reset the fields
		struct client *c = &clients[client_index];
		c->socket_fd = client_fd;
		c->parser_state = request_method;
		c->parser_tmp = 0;
		memset(c->request_uri_buf, 0, sizeof(c->request_uri_buf));
		c->request_uri_len = 0;
		memset(c->request_host_buf, 0, sizeof(c->request_host_buf));
		c->request_host_len = 0;
		memset(c->response_buf, 0, sizeof(c->response_buf));
		c->response_len = 0;
		c->bytes_sent = 0;

		// Setup the timeout
		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			perror("clock_gettime()");
			return false;
		}
		c->timeout = now;
		c->timeout.tv_sec += 2;

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

bool epoll_on_client_in(int client_index)
{
	struct client *c = &clients[client_index];

	bool continue_read = true;
	while (continue_read) {
		char receive_buffer[1024];

		int bytes_read =
		    read(c->socket_fd, receive_buffer, sizeof(receive_buffer));
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

		enum parser_result r =
		    client_continue_parsing(c, receive_buffer, bytes_read);
		switch (r) {
		case parser_result_continue:
			continue;
		case parser_result_failed:
			continue_read = false;
			// The socket FD will be removed from the epoll when it
			// is closed.
			close(c->socket_fd);
			client_free(client_index);
			break;
		case parser_result_can_respond:
			client_prepare_response(c);

			enum client_respond result =
			    client_continue_sending_response(c);
			switch (result) {
			case client_respond_continue: {
				struct epoll_event new_client_epoll;
				new_client_epoll.data.u64 = client_index + 1;
				// We need to wait until we can write to the
				// socket again.
				new_client_epoll.events =
				    EPOLLOUT | EPOLLET | EPOLLWAKEUP;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
					      c->socket_fd,
					      &new_client_epoll) == -1) {
					perror("epoll_"
					       "ctl()");
					return false;
				}

				continue_read = false;
				break;
			}
			case client_respond_failed:
				return false;
			case client_respond_finished:
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

	return true;
}

bool epoll_on_client_out(int client_index)
{
	struct client *c = &clients[client_index];

	enum client_respond result = client_continue_sending_response(c);
	switch (result) {
	case client_respond_continue:
		break;
	case client_respond_failed:
		return false;
	case client_respond_finished:
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

		int first_client_to_timeout = -1;
		int epoll_timeout = -1;

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!client_is_valid(i))
				continue;

			struct client *c = &clients[i];
			struct timespec *client_timeout = &c->timeout;
			if (now.tv_sec > client_timeout->tv_sec ||
			    (now.tv_sec == client_timeout->tv_sec &&
			     now.tv_nsec >= client_timeout->tv_nsec)) {
				// The request has already timed out
				close(c->socket_fd);
				client_free(i);
				continue;
			}

			time_t diff_sec = client_timeout->tv_sec - now.tv_sec;
			if (diff_sec > INT_MAX / 1000)
				continue;

			// Millisecond difference without whole seconds
			int tmp =
			    (client_timeout->tv_nsec - now.tv_nsec) / 1000000;

			int diff_msec = diff_sec * 1000;
			if (diff_msec > INT_MAX - tmp ||
			    diff_msec < INT_MIN + tmp)
				continue;
			diff_msec += tmp;

			if (diff_msec == 0)
				continue;

			if (epoll_timeout == -1 || diff_msec < epoll_timeout) {
				first_client_to_timeout = i;
				epoll_timeout = diff_msec;
			}
		}

		int ret =
		    epoll_wait(epoll_fd, events,
			       sizeof(events) / sizeof(*events), epoll_timeout);
		if (ret == 0) {
			// The client had a chance to send us something before
			// the timeout but it didn't do so, so we will close the
			// socket and free the client object.
			assert(first_client_to_timeout != -1);
			close(clients[first_client_to_timeout].socket_fd);
			client_free(first_client_to_timeout);
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
