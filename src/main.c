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

#include <stdnoreturn.h>

#include "cli.h"
#include "epoll.h"
#include "sys.h"
#include "util.h"

noreturn void main(const char *const *argv)
{
	struct cli_options options;
	options.server_port = 80;
	options.threads = 1;
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

	int server_fd =
	    sys_socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (server_fd < 0) {
		FPUTS_A(2, "socket() failed\n");
		sys_exit(1);
	}

	struct sockaddr_in addr;
	addr.sin_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(options.server_port);

	if (sys_bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		FPUTS_A(2, "bind() failed\n");
		sys_exit(1);
	}

	pid_t children[255];

	for (uint32_t i = 0; i < options.threads - 1; ++i) {
		pid_t child = sys_clone(CLONE_FILES | CLONE_FS | CLONE_IO |
					    CLONE_PARENT,
					NULL, NULL, NULL, 0);
		if (child < 0) {
			FPUTS_A(2, "clone() failed\n");

			/* Kill children that were created. */
			for (uint32_t j = 0; j < i; j++) {
				if (sys_kill(children[j], SIGKILL) != 0)
					FPUTS_A(2, "kill() failed\n");
			}

			sys_exit(1);
		} else if (child == 0) {
			/* Only the parent must clone itself, to prevent having
			   1 + (N - 1)! threads instead of just N threads. */
			break;
		}

		children[i] = child;
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
