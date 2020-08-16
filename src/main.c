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

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "cli.h"
#include "epoll.h"
#include "util.h"

int main(int argc, char **argv)
{
	struct cli_options options;
	options.server_port = 80;
	options.socket_backlog = 32;

	switch (cli_parse_args(&options, argc, argv)) {
	case CPR_SUCCESS:
		break;
	case CPR_STOP:
		/* For example, if the help message was requested. */
		return EXIT_SUCCESS;
	case CPR_ERROR:
		return EXIT_FAILURE;
	}

	int server_fd =
	    socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (server_fd == -1) {
		perror("socket()");
		return EXIT_FAILURE;
	}

	struct sockaddr_in addr = {0};
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(options.server_port);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind()");
		return EXIT_FAILURE;
	}

	if (!epoll_init(server_fd))
		return EXIT_FAILURE;

	if (listen(server_fd, options.socket_backlog) == -1) {
		perror("listen()");
		return EXIT_FAILURE;
	}

	for (;;) {
		if (!epoll_wait_and_dispatch())
			return EXIT_FAILURE;
	}
}
