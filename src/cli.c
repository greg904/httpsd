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

static bool cli_print_usage(int fd, const char *arg0);
static void cli_print_arg_out_of_range(const char *arg, const char *arg0);
static bool cli_parse_num(uint32_t *result, uint32_t min, uint32_t max,
			  const char *arg, const char *arg0);

enum cli_parse_result cli_parse_args(struct cli_options *options,
				     const char *const *argv)
{
	const char *arg0 = argv[0];

	/* Arguments start at 1. */
	++argv;

	for (; *argv != NULL; ++argv) {
		if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
			cli_print_usage(1, arg0);
			return CPR_STOP;
		} else if (strcmp(*argv, "-p") == 0 ||
			   strcmp(*argv, "--port") == 0) {
			if (!cli_parse_num(&options->server_port, 0, UINT16_MAX,
					   argv[1], arg0))
				return CPR_ERROR;
			++argv;
		} else if (strcmp(*argv, "-t") == 0 ||
			   strcmp(*argv, "--threads") == 0) {
			if (!cli_parse_num(&options->threads, 1, 256, argv[1],
					   arg0))
				return CPR_ERROR;
			++argv;
		} else if (strcmp(*argv, "-b") == 0 ||
			   strcmp(*argv, "--backlog") == 0) {
			if (!cli_parse_num(&options->socket_backlog, 1, INT_MAX,
					   argv[1], arg0))
				return CPR_ERROR;
			++argv;
		} else if (strcmp(*argv, "--") == 0) {
			/* Make sure that there is nothing after the double
			   hyphen because we do not accept any argument. */
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

static bool cli_print_usage(int fd, const char *arg0)
{
	return FPUTS_A(fd, "Usage: ") && FPUTS_0(fd, arg0) &&
	       FPUTS_A(
		   fd,
		   " [OPTION]...\nStarts an HTTP server that redirects "
		   "requests to "
		   "the same URL but with the HTTPS scheme instead, and drops "
		   "invalid requests.\n\n"
		   "  -p, --port=PORT       set port to start listening on\n"
		   "  -t, --threads=THREADS set amount of threads to use to "
		   "handle requests\n"
		   "  -b, --backlog=BACKLOG set maximum amount of connections "
		   "waiting to "
		   "be accepted\n"
		   "  -h, --help       display this help and exit\n");
}

static void cli_print_arg_out_of_range(const char *arg, const char *arg0)
{
	if (!FPUTS_0(2, arg0) || !FPUTS_A(2, ": number out of range: ") ||
	    !FPUTS_0(2, arg))
		return;
	FPUTS_A(2, "\n");
}

static bool cli_parse_num(uint32_t *result, uint32_t min, uint32_t max,
			  const char *arg, const char *arg0)
{
	if (arg == NULL) {
		if (!FPUTS_0(2, arg0) ||
		    !FPUTS_A(2, ": missing number for argument\n"))
			return false;

		return false;
	}

	*result = 0;

	for (; *arg != '\0'; ++arg) {
		if (*arg < '0' || *arg > '9') {
			if (!FPUTS_0(2, arg0) ||
			    !FPUTS_A(2, ": invalid number: ") ||
			    !FPUTS_0(2, arg) || !FPUTS_A(2, "\n"))
				return false;

			return false;
		}

		uint32_t to_add = *arg - '0';
		if (*result >= (max - to_add) / 10) {
			cli_print_arg_out_of_range(arg, arg0);
			return false;
		}

		*result *= 10;
		*result += to_add;
	}

	if (*result < min) {
		cli_print_arg_out_of_range(arg, arg0);
		return false;
	}

	return true;
}
