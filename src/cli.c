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

#include "cli.h"
#include "sys.h"
#include "util.h"

bool cli_print_usage(int fd, const char *arg0)
{
	return FPUTS_A(fd, "Usage: ") && FPUTS_0(fd, arg0) &&
	       FPUTS_A(
		   fd,
		   " [OPTION]...\nStarts an HTTP server that redirects "
		   "requests to "
		   "the same URL but with the HTTPS scheme instead, and drops "
		   "invalid requests.\n\n"
		   "  -p, --port=PORT  set port to start listening on\n"
		   "  -b, --backlog    set maximum amount of connections "
		   "waiting to "
		   "be accepted\n"
		   "  -h, --help       display this help and exit\n");
}

bool cli_parse_num(long *result, long max, const char *str, const char *arg0)
{
	*result = 0;

	for (; *str != '\0'; ++str) {
		if (*str < '0' || *str > '9') {
			if (!FPUTS_0(2, arg0) ||
			    !FPUTS_A(2, ": invalid number: ") ||
			    !FPUTS_0(2, str) || !FPUTS_A(2, "\n"))
				return false;

			return false;
		}

		*result *= 10;
		*result += *str - '0';

		if (*result > max)
			break;
	}

	if (*result > max) {
		if (!FPUTS_0(2, arg0) ||
		    !FPUTS_A(2, ": number out of range: ") ||
		    !FPUTS_0(2, str) || !FPUTS_A(2, "\n"))
			return false;

		return false;
	}

	return true;
}

enum cli_parse_result cli_parse_args(struct cli_options *options,
				     const char *const *argv)
{
	const char *arg0 = argv[0];

	/* Arguments start at 1. */
	++argv;

	for (; *argv != NULL; ++argv) {
		if (util_strcmp(*argv, "-h") || util_strcmp(*argv, "--help")) {
			cli_print_usage(0, arg0);
			return CPR_STOP;
		} else if (util_strcmp(*argv, "-p") ||
			   util_strcmp(*argv, "--port")) {
			if (argv[1] == NULL) {
				if (!FPUTS_0(2, arg0) ||
				    !FPUTS_A(
					2,
					": missing number for argument '-p'\n"))
					return false;

				return CPR_ERROR;
			}

			long tmp;
			if (!cli_parse_num(&tmp, UINT16_MAX, argv[1], arg0))
				return CPR_ERROR;
			options->server_port = tmp;

			++argv;
		} else if (util_strcmp(*argv, "-b") ||
			   util_strcmp(*argv, "--backlog")) {
			if (argv[1] == NULL) {
				if (!FPUTS_0(2, arg0) ||
				    !FPUTS_A(
					2,
					": missing number for argument '-b'\n"))
					return false;

				return CPR_ERROR;
			}

			long tmp;
			if (!cli_parse_num(&tmp, INT_MAX, argv[1], arg0))
				return CPR_ERROR;
			options->socket_backlog = tmp;

			++argv;
		} else if (util_strcmp(*argv, "--")) {
			/* Make sure that there is nothing after the double
			   slash because we do not accept any argument. */
			if (argv[1] != NULL) {
				cli_print_usage(2, arg0);
				return CPR_ERROR;
			}

			break;
		} else {
			cli_print_usage(2, arg0);
			return CPR_ERROR;
		}
	}

	return CPR_SUCCESS;
}
