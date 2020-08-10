// For accept4
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "client.h"
#include "epoll.h"
#include "misc.h"
#include "parser.h"

int epoll_fd;

bool epoll_setup() {
        epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1) {
		perror("epoll_create()");
		return false;
	}

        return true;
}

bool epoll_run() {
        for (;;) {
		struct epoll_event events[32];

		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			perror("clock_gettime()");
			return false;
		}
		uint64_t now_msec = now.tv_sec * 1000 + now.tv_nsec / 1000000;

		uint16_t clients_to_timeout = 0;
		int epoll_timeout = -1;

		for (int i = 0; i < client_get_max_index(); i++) {
			struct client *c = client_get(i);
			if (c == NULL)
				continue;
			
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
			for (int i = 0; i < client_get_max_index(); i++) {
				if ((clients_to_timeout & (1 << i)) != 0) {
					struct client *c = client_get(i);
					assert(c != NULL);

					close(c->socket_fd);
					client_free(i);
				}
			}
			continue;
		}
		if (ret == -1) {
			perror("epoll_wait()");
			return false;
		}

		for (int i = 0; i < ret; i++) {
			if (!epoll_on_event(&events[i]))
				return false;
		}
	}
}

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
		struct client *c = client_get(client_index);
		assert(c != NULL);

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

bool epoll_on_client_in(int client_index)
{
	struct client *c = client_get(client_index);
	assert(c != NULL);

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
			enum client_respond result =
			    client_continue_sending_response(c);
			switch (result) {
			case cr_continue: {
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
			case cr_error:
				return false;
			case cr_finished:
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

bool epoll_on_client_out(int client_index)
{
	struct client *c = client_get(client_index);
	assert(c != NULL);

	enum client_respond result = client_continue_sending_response(c);
	switch (result) {
	case cr_continue:
		break;
	case cr_error:
		return false;
	case cr_finished:
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
			// We haven't finished handling this request but there
			// was an error or the writing half has been closed so
			// now we will drop it because we can't do anything with
			// it.

			struct client *c = client_get(client_index);
			assert(c != NULL);

			close(c->socket_fd);
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

