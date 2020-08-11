#include <string.h>

#include "reqparser.h"
#include "util.h"

enum reqparser_type {
	/**
	 * Expects the GET method, and switches to RT_URI.
	 */
	RT_METHOD_0 = 0,

	/**
	 * Reads the request URI.
	 */
	RT_URI = 5,

	/**
	 * Ignores everything until it encounters a CR in which case it switches
	 * to RT_LF.
	 */
	RT_SKIP_LINE = 6,

	/**
	 * Expected a LF and switches to RT_HEADER_NAME_0.
	 */
	RT_LF = 7,

	/**
	 * Reads the header's name. Switches either to RT_HOST if the header's
	 * name is "Host" or to RT_SKIP_LINE if it is not.
	 */
	RT_HEADER_NAME_0 = 8,

	/**
	 * Reads the Host header's value and finishes parsing.
	 */
	RT_HOST = 14,
};

enum reqparser_sub {
	RS_COMPLETE,
	RS_CONTINUE,
	RS_EOF,
	RS_ERROR,
	RS_BUFFER_TOO_SMALL,
};

static enum reqparser_sub reqparser_method(struct reqparser_args *args);
static enum reqparser_sub reqparser_path(struct reqparser_args *args);
static enum reqparser_sub reqparser_skip_line(struct reqparser_args *args);
static enum reqparser_sub reqparser_lf(struct reqparser_args *args);
static enum reqparser_sub reqparser_header_name(struct reqparser_args *args);
static enum reqparser_sub reqparser_host(struct reqparser_args *args);

static void reqparser_fix_req_fields(struct reqparser_args *args, size_t old_host_index);

enum reqparser_completion reqparser_feed(struct reqparser_args *args)
{
	for (;;) {
		enum reqparser_sub r;

		switch (args->state) {
		case RT_METHOD_0:
		case RT_METHOD_0 + 1:
		case RT_METHOD_0 + 2:
		case RT_METHOD_0 + 3:
		case RT_METHOD_0 + 4:
			r = reqparser_method(args);
			break;
		case RT_URI:
			r = reqparser_path(args);
			break;
		case RT_SKIP_LINE:
			r = reqparser_skip_line(args);
			break;
		case RT_LF:
			r = reqparser_lf(args);
			break;
		case RT_HEADER_NAME_0:
		case RT_HEADER_NAME_0 + 1:
		case RT_HEADER_NAME_0 + 2:
		case RT_HEADER_NAME_0 + 3:
		case RT_HEADER_NAME_0 + 4:
		case RT_HEADER_NAME_0 + 5:
			r = reqparser_header_name(args);
			break;
		case RT_HOST:
			r = reqparser_host(args);
			break;
		}

		switch (r) {
		case RS_COMPLETE:
			return PC_COMPLETE;
		case RS_CONTINUE:
			continue;
		case RS_EOF:
			/* Everything parsed successfully. */
			return PC_NEEDS_MORE_DATA;
		case RS_ERROR:
			return PC_BAD_DATA;
		case RS_BUFFER_TOO_SMALL:
			return PC_BUFFER_TOO_SMALL;
		}
	}
}

static enum reqparser_sub reqparser_method(struct reqparser_args *args)
{
	const char method_str[] = "GET /";

	for (;;) {
		/* Invalid HTTP method */
		if (*args->data != method_str[args->state - RT_METHOD_0])
			return RS_ERROR;

		args->data++;
		args->state++;

		if (args->data == args->data_end)
			return RS_EOF;

		if (args->state == RT_URI)
			return RS_CONTINUE;
	}
}

static enum reqparser_sub reqparser_path(struct reqparser_args *args)
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
			return RS_ERROR;
		case ' ':
			args->req_fields[fill_index] = '\0';
			args->state = RT_SKIP_LINE;

			args->data++;
			if (args->data == args->data_end)
				return RS_EOF;

			return RS_CONTINUE;
		default:
			/* We need at least one NULL character after the path to
			   delimit it from the request Host header's value. */
			if (fill_index == args->req_fields_len - 2)
				return RS_BUFFER_TOO_SMALL;

			args->req_fields[fill_index] = ch;
			fill_index++;
		}

		args->data++;
		if (args->data == args->data_end)
			return RS_EOF;
	}
}

static enum reqparser_sub reqparser_skip_line(struct reqparser_args *args)
{
	for (;;) {
		if (*args->data == '\r') {
			args->state = RT_LF;

			args->data++;
			if (args->data == args->data_end)
				return RS_EOF;

			return RS_CONTINUE;
		}

		args->data++;
		if (args->data == args->data_end)
			return RS_EOF;
	}
}

static enum reqparser_sub reqparser_lf(struct reqparser_args *args)
{
	/* Expect the LF character. */
	if (*args->data != '\n')
		return RS_ERROR;

	args->state = RT_HEADER_NAME_0;

	args->data++;
	if (args->data == args->data_end)
		return RS_EOF;

	return RS_CONTINUE;
}

static enum reqparser_sub reqparser_header_name(struct reqparser_args *args)
{
	const char host_str[] = "Host: ";

	for (;;) {
		/* Check if it's not the Host header, in which case we can just
		   skip the entire line. */
		if (*args->data != host_str[args->state - RT_HEADER_NAME_0]) {
			args->state = RT_SKIP_LINE;

			args->data++;
			if (args->data == args->data_end)
				return RS_EOF;

			return RS_CONTINUE;
		}

		args->data++;
		args->state++;

		if (args->data == args->data_end)
			return RS_EOF;

		if (args->state == RT_HOST)
			return RS_CONTINUE;
	}
}

static enum reqparser_sub reqparser_host(struct reqparser_args *args)
{
	size_t fill_index = args->req_fields_len - 1;
	while (args->req_fields[fill_index] != '\0')
		fill_index--;

	for (;;) {
		char ch = *args->data;
		if (ch == '\r') {
			reqparser_fix_req_fields(args, fill_index + 1);
			return RS_COMPLETE;
		}

		/* We need at least one NULL character before the request Host
		   header's value to delimit it from path. */
		if (fill_index == 0 || args->req_fields[fill_index - 1] != '\0')
			return RS_BUFFER_TOO_SMALL;

		args->req_fields[fill_index] = ch;
		fill_index--;

		args->data++;
		args->state++;

		if (args->data == args->data_end)
			return RS_EOF;
	}
}

static void reqparser_fix_req_fields(struct reqparser_args *args, size_t old_host_index) {
	/* Now, reverse the host to put it back in the correct
	   order and move it against the request path, after the
	   NULL character. */

	util_reverse(args->req_fields + old_host_index, args->req_fields + args->req_fields_len - 1);

	size_t null_index = 0;
	while (args->req_fields[null_index] != '\0')
		null_index++;

	size_t host_len = args->req_fields_len - old_host_index;
	if (host_len != 0)
		memmove(args->req_fields + null_index + 1,
			args->req_fields + old_host_index,
			host_len);

	/* Finally, add the NULL character at the end to delimit
		the end of the host. */
	if ((null_index + 1) + host_len != args->req_fields_len)
		args->req_fields[(null_index + 1) + host_len] =
			'\0';
}
