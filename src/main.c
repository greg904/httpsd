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

#include "cli.h"
#include "epoll.h"
#include "sys.h"
#include "util.h"

__attribute__((noreturn)) void main(const char *const *argv)
{
	struct cli_options options;
	options.server_port = 80;
	options.socket_backlog = 32;

	switch (cli_parse_args(&options, argv)) {
	case CPR_SUCCESS:
		break;
	case CPR_STOP:
		/* For example, if the help message was requested. */
		sys_exit(0);
	case CPR_ERROR:
		sys_exit(1);
	}

	int server_fd = sys_socket(
	    SYS_AF_INET, SYS_SOCK_STREAM | SYS_SOCK_CLOEXEC | SYS_SOCK_NONBLOCK,
	    0);
	if (server_fd < 0) {
		FPUTS_A(2, "socket() failed\n");
		sys_exit(1);
	}

	struct sys_sockaddr_in addr;
	addr.sin_addr = SYS_INADDR_ANY;
	addr.sin_family = SYS_AF_INET;
	addr.sin_port = util_htob16(options.server_port);

	if (sys_bind(server_fd, (struct sys_sockaddr *)&addr, sizeof(addr)) !=
	    0) {
		FPUTS_A(2, "bind() failed\n");
		sys_exit(1);
	}

	if (!epoll_init(server_fd))
		sys_exit(1);

	if (sys_listen(server_fd, options.socket_backlog) != 0) {
		FPUTS_A(2, "listen() failed");
		sys_exit(1);
	}

	for (;;) {
		if (!epoll_wait_and_dispatch())
			sys_exit(1);
	}
}
