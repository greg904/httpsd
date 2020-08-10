#include <string.h>

#include "parser.h"
#include "util.h"

enum parse_type {
	/**
	 * Expects the GET method, and switches to PS_URI.
	 */
	PS_METHOD_0 = 0,

	/**
	 * Reads the request URI.
	 */
	PS_URI = 5,

	/**
	 * Ignores everything until it encounters a CR in which case it switches
	 * to PS_LF.
	 */
	PS_IGNORE_LINE = 6,

	/**
	 * Expected a LF and switches to PS_HEADER_NAME_0.
	 */
	PS_LF = 7,

	/**
	 * Reads the header's name. Switches either to PS_HOST if the header's
	 * name is "Host" or to PS_IGNORE_LINE if it is not.
	 */
	PS_HEADER_NAME_0 = 8,

	/**
	 * Reads the Host header's value and finishes parsing.
	 */
	PS_HOST = 14,
};

enum parser_internal {
	PI_CONTINUE,
	PI_EOF,
	PI_ERROR,
	PI_COMPLETE,
};

static enum parser_internal parser_go_method(struct parse_args *args);
static enum parser_internal parser_go_path(struct parse_args *args);
static enum parser_internal parser_go_ignore_line(struct parse_args *args);
static enum parser_internal parser_go_lf(struct parse_args *args);
static enum parser_internal parser_go_header_name(struct parse_args *args);
static enum parser_internal parser_go_host(struct parse_args *args);

enum parse_completion parser_go(struct parse_args *args)
{
	for (;;) {
		enum parser_internal r;

		switch (args->state) {
		case PS_METHOD_0:
		case PS_METHOD_0 + 1:
		case PS_METHOD_0 + 2:
		case PS_METHOD_0 + 3:
		case PS_METHOD_0 + 4:
			r = parser_go_method(args);
			break;
		case PS_URI:
			r = parser_go_path(args);
			break;
		case PS_IGNORE_LINE:
			r = parser_go_ignore_line(args);
			break;
		case PS_LF:
			r = parser_go_lf(args);
			break;
		case PS_HEADER_NAME_0:
		case PS_HEADER_NAME_0 + 1:
		case PS_HEADER_NAME_0 + 2:
		case PS_HEADER_NAME_0 + 3:
		case PS_HEADER_NAME_0 + 4:
		case PS_HEADER_NAME_0 + 5:
			r = parser_go_header_name(args);
			break;
		case PS_HOST:
			r = parser_go_host(args);
			break;
		}

		switch (r) {
		case PI_CONTINUE:
			continue;
		case PI_EOF:
			/* Everything parsed successfully. */
			return PC_NEEDS_MORE_DATA;
		case PI_ERROR:
			return PC_ERROR;
		case PI_COMPLETE:
			return PC_COMPLETE;
		}
	}
}

static enum parser_internal parser_go_method(struct parse_args *args)
{
	const char method_str[] = "GET /";

	for (;;) {
		/* Invalid HTTP method */
		if (*args->data != method_str[args->state - PS_METHOD_0])
			return PI_ERROR;

		args->data++;
		args->state++;

		if (args->data == args->data_end)
			return PI_EOF;

		if (args->state == PS_URI)
			return PI_CONTINUE;
	}
}

static enum parser_internal parser_go_path(struct parse_args *args)
{
	size_t fill_index = 0;
	while (args->req_fields[fill_index] != '\0')
		fill_index++;

	for (;;) {
		char ch = *args->data;
		switch (ch) {
		case '\0':
			/* We can't accept this character because we use it
			   internally to delimit the end of the path and the
			   start of the request Host header's value. */
			return PI_ERROR;
		case ' ':
			args->req_fields[fill_index] = '\0';
			args->state = PS_IGNORE_LINE;

			args->data++;
			if (args->data == args->data_end)
				return PI_EOF;

			return PI_CONTINUE;
		default:
			/* We need at least one NULL character after the path to
			   delimit it from the request Host header's value. */
			if (fill_index == args->req_fields_len - 2)
				return PI_ERROR;

			args->req_fields[fill_index] = ch;
			fill_index++;
		}

		args->data++;
		if (args->data == args->data_end)
			return PI_EOF;
	}
}

static enum parser_internal parser_go_ignore_line(struct parse_args *args)
{
	for (;;) {
		if (*args->data == '\r') {
			args->state = PS_LF;

			args->data++;
			if (args->data == args->data_end)
				return PI_EOF;

			return PI_CONTINUE;
		}

		args->data++;
		if (args->data == args->data_end)
			return PI_EOF;
	}
}

static enum parser_internal parser_go_lf(struct parse_args *args)
{
	/* Expect the LF character. */
	if (*args->data != '\n')
		return PI_ERROR;

	args->state = PS_HEADER_NAME_0;

	args->data++;
	if (args->data == args->data_end)
		return PI_EOF;

	return PI_CONTINUE;
}

static enum parser_internal parser_go_header_name(struct parse_args *args)
{
	const char host_str[] = "Host: ";

	for (;;) {
		/* Check if it's not the Host header, in which case we can just
		   skip the entire line. */
		if (*args->data != host_str[args->state - PS_HEADER_NAME_0]) {
			args->state = PS_IGNORE_LINE;

			args->data++;
			if (args->data == args->data_end)
				return PI_EOF;

			return PI_CONTINUE;
		}

		args->data++;
		args->state++;

		if (args->data == args->data_end)
			return PI_EOF;

		if (args->state == PS_HOST)
			return PI_CONTINUE;
	}
}

static enum parser_internal parser_go_host(struct parse_args *args)
{
	size_t fill_index = args->req_fields_len - 1;
	while (args->req_fields[fill_index] != '\0')
		fill_index--;

	for (;;) {
		char ch = *args->data;
		if (ch == '\r') {
			/* Now, reverse the host to put it back in the correct
			   order and move it against the request path, after the
			   NULL character. */

			util_reverse(args->req_fields + fill_index + 1,
				     args->req_fields + args->req_fields_len -
					 1);

			size_t null_index = 0;
			while (args->req_fields[null_index] != '\0')
				null_index++;

			size_t host_len = args->req_fields_len - fill_index - 1;
			if (host_len != 0)
				memmove(args->req_fields + null_index + 1,
					args->req_fields + fill_index + 1,
					host_len);

			/* Finally, add the NULL character at the end to delimit
			   the end of the host. */
			if ((null_index + 1) + host_len != args->req_fields_len)
				args->req_fields[(null_index + 1) + host_len] =
				    '\0';

			return PI_COMPLETE;
		}

		/* We need at least one NULL character before the request Host
		   header's value to delimit it from path. */
		if (fill_index == 0)
			return PI_ERROR;

		args->req_fields[fill_index] = ch;
		args->req_fields[fill_index - 1] = '\0';
		fill_index--;

		args->data++;
		args->state++;

		if (args->data == args->data_end)
			return PI_EOF;
	}
}
