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

#ifndef G9_HTTPS_REDIRECT_CLI_H
#define G9_HTTPS_REDIRECT_CLI_H

#include <stdint.h>

struct cli_options {
	uint16_t server_port;
	int socket_backlog;
};

enum cli_parse_result {
	CPR_SUCCESS,
	CPR_STOP,
	CPR_ERROR,
};

enum cli_parse_result cli_parse_args(struct cli_options *options, int argc,
				     char *const *argv);

#endif
