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

#include "epoll.h"
#include "util.h"

#ifndef SERVER_PORT
#	define SERVER_PORT 8080
#endif

#ifndef SOCKET_BACKLOG
#	define SOCKET_BACKLOG 32
#endif

int main(int argc, char **argv)
{
	if (argc > 1) {
		fputs("Usage: ", stderr);
		fputs(argv[0], stderr);
		fputs("\nStarts an HTTP server on port " STR_VALUE_MACRO(
			  SERVER_PORT) " that redirects requests with the GET "
				       "method to the same URL but with the "
				       "HTTPS scheme instead, and drops all "
				       "other requests.\n",
		      stderr);
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
	addr.sin_port = htons(SERVER_PORT);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind()");
		return EXIT_FAILURE;
	}

	if (!epoll_init(server_fd))
		return EXIT_FAILURE;

	if (listen(server_fd, SOCKET_BACKLOG) == -1) {
		perror("listen()");
		return EXIT_FAILURE;
	}

	for (;;) {
		if (!epoll_wait_and_dispatch())
			return EXIT_FAILURE;
	}
}
