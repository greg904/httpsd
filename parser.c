#include <string.h>

#include "parser.h"

enum _parser_internal_result {
	_pir_continue,
	_pir_eof,
	_pir_error,
	_pir_finished,
};

static enum _parser_internal_result _parser_do_method(uint8_t *req_parser_state,
						      char *req_fields,
						      size_t req_fields_len,
						      const char **data,
						      const char *data_end);
static enum _parser_internal_result
_parser_do_uri(uint8_t *req_parser_state, char *req_fields,
	       size_t req_fields_len, const char **data, const char *data_end);
static enum _parser_internal_result
_parser_do_ignore_line(uint8_t *req_parser_state, char *req_fields,
		       size_t req_fields_len, const char **data,
		       const char *data_end);
static enum _parser_internal_result
_parser_do_lf(uint8_t *req_parser_state, char *req_fields,
	      size_t req_fields_len, const char **data, const char *data_end);
static enum _parser_internal_result
_parser_do_header_name(uint8_t *req_parser_state, char *req_fields,
		       size_t req_fields_len, const char **data,
		       const char *data_end);
static enum _parser_internal_result
_parser_do_host(uint8_t *req_parser_state, char *req_fields,
		size_t req_fields_len, const char **data, const char *data_end);

static void _reverse(char *data, char *data_end);

enum parser_result parser_go(uint8_t *req_parser_state, char *req_fields,
			     size_t req_fields_len, const char *data,
			     size_t len)
{
	const char *data_end = data + len;

	for (;;) {
		enum _parser_internal_result r;

		switch (*req_parser_state) {
		case ps_method_0:
		case ps_method_0 + 1:
		case ps_method_0 + 2:
		case ps_method_0 + 3:
		case ps_method_0 + 4:
			r = _parser_do_method(req_parser_state, req_fields,
					      req_fields_len, &data, data_end);
			break;
		case ps_uri:
			r = _parser_do_uri(req_parser_state, req_fields,
					   req_fields_len, &data, data_end);
			break;
		case ps_ignore_line:
			r = _parser_do_ignore_line(req_parser_state, req_fields,
						   req_fields_len, &data,
						   data_end);
			break;
		case ps_lf:
			r = _parser_do_lf(req_parser_state, req_fields,
					  req_fields_len, &data, data_end);
			break;
		case ps_header_name_0:
		case ps_header_name_0 + 1:
		case ps_header_name_0 + 2:
		case ps_header_name_0 + 3:
		case ps_header_name_0 + 4:
		case ps_header_name_0 + 5:
			r = _parser_do_header_name(req_parser_state, req_fields,
						   req_fields_len, &data,
						   data_end);
			break;
		case ps_host:
			r = _parser_do_host(req_parser_state, req_fields,
					    req_fields_len, &data, data_end);
			break;
		}

		switch (r) {
		case _pir_continue:
			continue;
		case _pir_eof:
			// Everything parsed successfully.
			return pr_continue;
		case _pir_error:
			return pr_error;
		case _pir_finished:
			return pr_finished;
		}
	}
}

static enum _parser_internal_result _parser_do_method(uint8_t *req_parser_state,
						      char *req_fields,
						      size_t req_fields_len,
						      const char **data,
						      const char *data_end)
{
	const char method_str[] = "GET /";

	for (;;) {
		// Invalid HTTP method
		if (**data != method_str[*req_parser_state - ps_method_0])
			return _pir_error;

		(*data)++;
		(*req_parser_state)++;

		if (*data == data_end)
			return _pir_eof;

		if (*req_parser_state == ps_uri)
			return _pir_continue;
	}
}

static enum _parser_internal_result
_parser_do_uri(uint8_t *req_parser_state, char *req_fields,
	       size_t req_fields_len, const char **data, const char *data_end)
{
	size_t fill_index = 0;
	while (req_fields[fill_index] != '\0')
		fill_index++;

	for (;;) {
		char ch = **data;
		switch (ch) {
		case '\0':
			// We can't accept this character because we use it
			// internally to delimit the end of the URI and the
			// start of the request Host header's value.
			return _pir_error;
		case ' ':
			req_fields[fill_index] = '\0';
			*req_parser_state = ps_ignore_line;

			(*data)++;
			if (*data == data_end)
				return _pir_eof;

			return _pir_continue;
		default:
			// We need at least one NULL character after the URI to
			// delimit the request Host header's value from the URI
			// value.
			if (fill_index == req_fields_len - 2)
				return _pir_error;

			req_fields[fill_index] = ch;
			fill_index++;
		}

		(*data)++;
		if (*data == data_end)
			return _pir_eof;
	}
}

static enum _parser_internal_result
_parser_do_ignore_line(uint8_t *req_parser_state, char *req_fields,
		       size_t req_fields_len, const char **data,
		       const char *data_end)
{
	for (;;) {
		if (**data == '\r') {
			*req_parser_state = ps_lf;

			(*data)++;
			if (*data == data_end)
				return _pir_eof;

			return _pir_continue;
		}

		(*data)++;
		if (*data == data_end)
			return _pir_eof;
	}
}

static enum _parser_internal_result
_parser_do_lf(uint8_t *req_parser_state, char *req_fields,
	      size_t req_fields_len, const char **data, const char *data_end)
{
	// Expect the LF character.
	if (**data != '\n')
		return _pir_error;

	*req_parser_state = ps_header_name_0;

	(*data)++;
	if (*data == data_end)
		return _pir_eof;

	return _pir_continue;
}

static enum _parser_internal_result
_parser_do_header_name(uint8_t *req_parser_state, char *req_fields,
		       size_t req_fields_len, const char **data,
		       const char *data_end)
{
	const char host_str[] = "Host: ";

	for (;;) {
		// Check if it's not the Host header, in which case we can just
		// skip the entire line.
		if (**data != host_str[*req_parser_state - ps_header_name_0]) {
			*req_parser_state = ps_ignore_line;

			(*data)++;
			if (*data == data_end)
				return _pir_eof;

			return _pir_continue;
		}

		(*data)++;
		(*req_parser_state)++;

		if (*data == data_end)
			return _pir_eof;

		if (*req_parser_state == ps_host)
			return _pir_continue;
	}
}

#include <stdio.h>

static enum _parser_internal_result
_parser_do_host(uint8_t *req_parser_state, char *req_fields,
		size_t req_fields_len, const char **data, const char *data_end)
{
	size_t fill_index = req_fields_len - 1;
	while (req_fields[fill_index] != '\0')
		fill_index--;

	for (;;) {
		char ch = **data;
		if (ch == '\r') {
			// Now, reverse the host to put it back in the correct
			// order and move it against the request, after the NULL
			// character.

			_reverse(req_fields + fill_index + 1,
				 req_fields + req_fields_len - 1);

			size_t null_index = 0;
			while (req_fields[null_index] != '\0')
				null_index++;

			size_t host_len = req_fields_len - fill_index - 1;
			if (host_len != 0)
				memmove(req_fields + null_index + 1,
					req_fields + fill_index + 1, host_len);

			// Finally, add the NULL character at the end to delimit
			// the end of the host.
			if ((null_index + 1) + host_len != req_fields_len)
				req_fields[(null_index + 1) + host_len] = '\0';

			return _pir_finished;
		}

		// We need at least one NULL character before the Host header's
		// value to delimmit the request Host header's value from the
		// URI value.
		if (fill_index == 0)
			return _pir_error;

		req_fields[fill_index] = ch;
		req_fields[fill_index - 1] = '\0';
		fill_index--;

		(*data)++;
		(*req_parser_state)++;

		if (*data == data_end)
			return _pir_eof;
	}
}

static void _reverse(char *data, char *data_end)
{
	while (data < data_end) {
		// Swap bytes
		char tmp = *data;
		*data = *data_end;
		*data_end = tmp;

		data++;
		data_end--;
	}
}
