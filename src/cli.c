#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"

void cli_print_usage(FILE *stream, const char *argv0)
{
	fputs("Usage: ", stream);
	fputs(argv0, stream);
	fputs(" [OPTION]...\nStarts an HTTP server that redirects requests to "
	      "the same URL but with the HTTPS scheme instead, and drops "
	      "invalid requests.\n\n"
	      "  -p, --port=PORT  set port to start listening on\n"
	      "  -b, --backlog    set maximum amount of connections waiting to "
	      "be accepted\n"
	      "  -h, --help       display this help and exit\n",
	      stream);
}

bool cli_parse_num(long *result, long min, long max, const char *argv0)
{
	/* Reset errno so that we know if strtol failed. */
	errno = 0;

	char *tmp;
	*result = strtol(optarg, &tmp, 10);

	if (errno != 0 || tmp != optarg + strlen(optarg)) {
		fputs(argv0, stderr);
		fputs(": invalid number: ", stderr);
		fputs(optarg, stderr);
		fputc('\n', stderr);
		return false;
	}

	if (*result < min || *result > max) {
		fputs(argv0, stderr);
		fputs(": number out of range: ", stderr);
		fputs(optarg, stderr);
		fputc('\n', stderr);
		return false;
	}

	return true;
}

enum cli_parse_result cli_parse_args(struct cli_options *options, int argc,
				     char *const *argv)
{
	for (;;) {
		struct option long_options[] = {
		    {"port", required_argument, NULL, 'p'},
		    {"backlog", required_argument, NULL, 'b'},
		    {"help", no_argument, NULL, 'h'},
		};
		char ch = getopt_long(argc, argv, "p:b:h", long_options, NULL);
		if (ch == -1)
			break;

		switch (ch) {
		case 'p': {
			long tmp;
			if (!cli_parse_num(&tmp, 0, UINT16_MAX, argv[0]))
				return CPR_ERROR;
			options->server_port = tmp;
			break;
		}
		case 'b': {
			long tmp;
			if (!cli_parse_num(&tmp, 0, INT_MAX, argv[0]))
				return CPR_ERROR;
			options->socket_backlog = tmp;
			break;
		}
		case 'h':
			cli_print_usage(stdout, argv[0]);
			return CPR_STOP;
		case '?':
			cli_print_usage(stderr, argv[0]);
			return CPR_ERROR;
		}
	}

	return CPR_SUCCESS;
}
