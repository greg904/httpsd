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

#ifndef HTTP2SD_EPOLL_H
#define HTTP2SD_EPOLL_H

#include <stdbool.h>

/**
 * Initializes the epoll module. Takes the HTTP server socket's FD as an
 * argument.
 */
bool epoll_init(int server_socket_fd);

/**
 * Blocks until something is worth doing and does it.
 */
bool epoll_wait_and_dispatch();

#endif
