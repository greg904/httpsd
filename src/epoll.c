/*
 * Copyright (C) 2020 Greg Depoire--Ferrer <greg.depoire@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "conn.h"
#include "epoll.h"
#include "util.h"

static int epoll_fd;
static int epoll_server_socket_fd;

static uint64_t epoll_now;
static int epoll_max_sleep;

struct sys_epoll_event epoll_event_buffer[32];

static bool epoll_on_event(const struct sys_epoll_event *event);
static bool epoll_on_server_in();
static bool epoll_on_conn_in(int conn_id);
static bool epoll_on_conn_out(int conn_id);

static void epoll_timeout_helper(int conn_id);

bool epoll_init(int server_socket_fd)
{
	epoll_server_socket_fd = server_socket_fd;

	epoll_fd = sys_epoll_create1(SYS_EPOLL_CLOEXEC);
	if (epoll_fd < 0) {
		FPUTS_A(2, "epoll_create() failed\n");
		return false;
	}

	struct sys_epoll_event server_epoll_event;
	server_epoll_event.data.u64 = 0;
	/* We want to be notified when the server socket is ready to accept a
	   client socket. */
	server_epoll_event.events = SYS_EPOLLIN | SYS_EPOLLWAKEUP;

	if (sys_epoll_ctl(epoll_fd, SYS_EPOLL_CTL_ADD, epoll_server_socket_fd,
			  &server_epoll_event) != 0) {
		FPUTS_A(2, "epoll_ctl() failed\n");
		return false;
	}

	return true;
}

bool epoll_wait_and_dispatch()
{
	struct sys_timespec now_ts;
	if (sys_clock_gettime(SYS_CLOCK_MONOTONIC, &now_ts) != 0) {
		FPUTS_A(2, "clock_gettime() failed\n");
		return false;
	}
	epoll_now = now_ts.tv_sec * 1000 + now_ts.tv_nsec / 1000000;
	epoll_max_sleep = -1;
	conn_for_each(epoll_timeout_helper);

	int ret = sys_epoll_wait(epoll_fd, epoll_event_buffer,
				 sizeof(epoll_event_buffer) /
				     sizeof(*epoll_event_buffer),
				 epoll_max_sleep);
	if (ret == 0) {
		/* One of the connections has exceeded its timeout, so
		   we will close it automatically in the next
		   iteration. */
		return true;
	}
	if (ret < 0) {
		FPUTS_A(2, "epoll_wait() failed\n");
		return false;
	}

	for (int i = 0; i < ret; i++) {
		if (!epoll_on_event(&epoll_event_buffer[i]))
			return false;
	}

	return true;
}

static bool epoll_on_event(const struct sys_epoll_event *event)
{
	bool in = (event->events & SYS_EPOLLIN) != 0;
	bool out = (event->events & SYS_EPOLLOUT) != 0;
	bool rdhup = (event->events & SYS_EPOLLRDHUP) != 0;
	bool err = (event->events & SYS_EPOLLERR) != 0;

	if (event->data.u64 == 0) {
		if (!epoll_on_server_in())
			return false;
	} else {
		int conn_id = (int)(event->data.u64 - 1);

		if (rdhup || err) {
			/* We haven't finished handling this request but there
			   was an error or the writing half has been closed so
			   now we will drop it because we can't do anything with
			   it. */

			sys_close(conn_get_socket_fd(conn_id));
			conn_free(conn_id);

			return true;
		}

		/* This should never fail because we first register for EPOLLIN
		   and then we register for just EPOLLOUT as soon as we want to
		   send the response, so we should never have both of them at
		   the same time. */
		ASSERT(in != out);

		if (in && !epoll_on_conn_in(conn_id))
			return false;
		if (out && !epoll_on_conn_out(conn_id))
			return false;
	}

	return true;
}

static bool epoll_on_server_in()
{
	/* The server socket is ready to accept one or more connection(s). */

	while (!conn_is_full()) {
		int client_fd =
		    sys_accept4(epoll_server_socket_fd, NULL, NULL,
				SYS_SOCK_CLOEXEC | SYS_SOCK_NONBLOCK);
		if (client_fd < 0) {
			if (client_fd == -SYS_EAGAIN) {
				/* We have already accepted all connections. */
				break;
			}

			FPUTS_A(2, "accept() failed\n");
			return false;
		}

		int conn_id = conn_new(client_fd);
		ASSERT(conn_id != -1);

		/* Setup the timeout */
		struct sys_timespec now;
		if (sys_clock_gettime(SYS_CLOCK_MONOTONIC, &now) != 0) {
			FPUTS_A(2, "clock_gettime() failed\n");
			return false;
		}
		uint64_t timeout =
		    now.tv_sec * 1000 + now.tv_nsec / 1000000 + 2000;
		conn_set_timeout(conn_id, timeout);

		struct sys_epoll_event client_epoll_event;
		client_epoll_event.data.u64 = conn_id + 1;
		/* For now, we only care about reading the request. Later, when
		   we want to know when we can write to the socket to respond to
		   the request, we will call epoll_ctl to modify the events. */
		client_epoll_event.events =
		    SYS_EPOLLIN | SYS_EPOLLET | SYS_EPOLLWAKEUP;

		if (sys_epoll_ctl(epoll_fd, SYS_EPOLL_CTL_ADD, client_fd,
				  &client_epoll_event) != 0) {
			FPUTS_A(2, "epoll_ctl() failed\n");
			return false;
		}
	}

	return true;
}

static bool epoll_on_conn_in(int conn_id)
{
	int socket_fd = conn_get_socket_fd(conn_id);

	for (;;) {
		int bytes_read =
		    sys_read(socket_fd, util_tmp_buf, sizeof(util_tmp_buf));
		if (bytes_read < 0) {
			if (bytes_read == -SYS_EAGAIN) {
				/* We have already read everything. */
				return true;
			}

			FPUTS_A(2, "read() failed\n");
			return false;
		}
		if (bytes_read == 0) {
			/* EOS before we finished parsing, so this is an invalid
			   request. We will close the client's socket and forget
			   about it. */
			sys_close(socket_fd);
			conn_free(conn_id);
			return true;
		}

		enum conn_wants_more wants_more =
		    conn_recv(conn_id, util_tmp_buf, bytes_read);
		switch (wants_more) {
		case CWM_YES:
			continue;
		case CWM_NO:
			/* Now, we know what to put in the HTTP response and we
			   might even be able to send it because the socket
			   might already be writable, so let's try it. */
			switch (conn_send(conn_id)) {
			case CWM_YES: {
				struct sys_epoll_event new_client_epoll;
				new_client_epoll.data.u64 = conn_id + 1;
				/* We need to wait until we can write to the
				   socket again. */
				new_client_epoll.events = SYS_EPOLLOUT |
							  SYS_EPOLLET |
							  SYS_EPOLLWAKEUP;
				if (sys_epoll_ctl(epoll_fd, SYS_EPOLL_CTL_MOD,
						  socket_fd,
						  &new_client_epoll) != 0) {
					FPUTS_A(2, "epoll_ctl() failed\n");
					return false;
				}
				return true;
			}
			case CWM_NO:
				/* We're already done! */
				sys_close(socket_fd);
				conn_free(conn_id);
				return true;
			case CWM_ERROR:
				return false;
			}
		case CWM_ERROR:
			/* The socket FD will be removed from the epoll when it
			   is closed. */
			sys_close(socket_fd);
			conn_free(conn_id);
			return true;
		}
	}

	return true;
}

static bool epoll_on_conn_out(int conn_id)
{
	switch (conn_send(conn_id)) {
	case CWM_YES:
		return true;
	case CWM_NO:
		/* We're done. */
		sys_close(conn_get_socket_fd(conn_id));
		conn_free(conn_id);
		return true;
	case CWM_ERROR:
		return false;
	}
}

static void epoll_timeout_helper(int conn_id)
{
	uint64_t conn_timeout = conn_get_timeout(conn_id);

	if (epoll_now >= conn_timeout) {
		sys_close(conn_get_socket_fd(conn_id));
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
