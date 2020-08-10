// For accept4
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "conn.h"
#include "epoll.h"
#include "misc.h"
#include "parser.h"

int epoll_fd;

static uint64_t epoll_now;
static int epoll_max_sleep;

static void epoll_timeout_helper(int conn_id);

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

		struct timespec now_ts;
		if (clock_gettime(CLOCK_MONOTONIC, &now_ts) == -1) {
			perror("clock_gettime()");
			return false;
		}
		epoll_now = now_ts.tv_sec * 1000 + now_ts.tv_nsec / 1000000;
		epoll_max_sleep = -1;
		conn_for_each(epoll_timeout_helper);

		int ret =
		    epoll_wait(epoll_fd, events,
			       sizeof(events) / sizeof(*events), epoll_max_sleep);
		if (ret == 0) {
			/* One of the connections has exceeded its timeout, so
			   we will close it automatically in the next
			   iteration. */
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
	/* The server socket is ready to accept one or many connection(s). */

	for (;;) {
		int client_fd = accept4(server_fd, NULL, NULL,
					SOCK_CLOEXEC | SOCK_NONBLOCK);
		if (client_fd == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				/* We have already accepted all connections. */
				break;
			}

			perror("accept()");
			return false;
		}

		int conn_id = conn_new(client_fd);
		if (conn_id == -1) {
			/* Too many clients right now, sorry! */
			close(client_fd);
			continue;
		}

		/* Setup the timeout */
		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			perror("clock_gettime()");
			return false;
		}
		uint64_t timeout = now.tv_sec * 1000 + now.tv_nsec / 1000000 + 2000;
		conn_set_timeout(conn_id, timeout);

		struct epoll_event client_epoll_event;
		client_epoll_event.data.u64 = conn_id + 1;
		/* For now, we only care about reading the request. Later, when
		   we want to know when we can write to the socket to respond to
		   the request, we will call epoll_ctl to modify the events. */
		client_epoll_event.events = EPOLLIN | EPOLLET | EPOLLWAKEUP;

		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd,
			      &client_epoll_event) == -1) {
			perror("epoll_ctl()");
			return false;
		}
	}

	return true;
}

bool epoll_on_conn_in(int conn_id)
{
	int socket_fd = conn_get_socket_fd(conn_id);

	for (;;) {
		int bytes_read = read(socket_fd, tmp_buf, sizeof(tmp_buf));
		if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				/* We have already read everything. */
				return true;
			}

			perror("read()");
			return false;
		}
		if (bytes_read == 0) {
			/* EOS before we finished parsing, so this is an invalid
			   request. We will close the client's socket and forget
			   about it. */
			close(socket_fd);
			conn_free(conn_id);
			return true;
		}

		enum conn_wants_more wants_more = conn_recv(conn_id, tmp_buf, bytes_read);
		switch (wants_more) {
		case CWM_YES:
			continue;
		case CWM_NO:
			/* Now, we know what to put in the HTTP response and we
			   might even be able to send it because the socket
			   might already be writable, so let's try it. */
			switch (conn_send(conn_id)) {
			case CWM_YES: {
				struct epoll_event new_client_epoll;
				new_client_epoll.data.u64 = conn_id + 1;
				/* We need to wait until we can write to the
				   socket again. */
				new_client_epoll.events =
				    EPOLLOUT | EPOLLET | EPOLLWAKEUP;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
					      socket_fd,
					      &new_client_epoll) == -1) {
					perror("epoll_ctl()");
					return false;
				}
				return true;
			}
			case CWM_NO:
				/* We're already done! */
				close(socket_fd);
				conn_free(conn_id);
				return true;
			case CWM_ERROR:
				return false;
			}
		case CWM_ERROR:
			/* The socket FD will be removed from the epoll when it
			   is closed. */
			close(socket_fd);
			conn_free(conn_id);
			return true;
		}
	}

	return true;
}

bool epoll_on_conn_out(int conn_id)
{
	switch (conn_send(conn_id)) {
	case CWM_YES:
		return true;
	case CWM_NO:
		/* We're done. */
		close(conn_get_socket_fd(conn_id));
		conn_free(conn_id);
		return true;
	case CWM_ERROR:
		return false;
	}
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
		int conn_id = event->data.u64 - 1;

		if (rdhup || err) {
			/* We haven't finished handling this request but there
			   was an error or the writing half has been closed so
			   now we will drop it because we can't do anything with
			   it. */

			close(conn_get_socket_fd(conn_id));
			conn_free(conn_id);

			return true;
		}

		/* This should never fail because we first
		   register for EPOLLIN and then we register for
		   just EPOLLOUT as soon as we want to send the
		   response, so we should never have both of
		   them at the same time. */
		assert(in != out);

		if (in && !epoll_on_conn_in(conn_id))
			return false;
		if (out && !epoll_on_conn_out(conn_id))
			return false;
	}

	return true;
}

static void epoll_timeout_helper(int conn_id) {
	uint64_t conn_timeout = conn_get_timeout(conn_id);

	if (epoll_now >= conn_timeout) {
		close(conn_get_socket_fd(conn_id));
		conn_free(conn_id);
		return;
	}

	if (conn_timeout - epoll_now > INT_MAX)
		return;
	int tmp = conn_timeout - epoll_now;

	/* Find the first connection's timeout that is will happen in the future
	   and set the epoll maximum sleep time to that timeout so that if the
	   timeout happens, we will exit the epoll_wait call and be able to
	   drop the connection. */
	if (epoll_max_sleep == -1 || tmp < epoll_max_sleep)
		epoll_max_sleep = tmp;
}
