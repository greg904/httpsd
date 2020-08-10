#ifndef G9_HTTPS_REDIRECT_PARSER_H
#define G9_HTTPS_REDIRECT_PARSER_H

#include <stdint.h>
#include <stdlib.h>

enum parser_state {
	// Expects the GET method, and switches to ps_uri.
	ps_method_0 = 0,

	// Reads the request URI.
	ps_uri = 5,

	// Ignores everything until it encounters a CR in which case it switches
	// to ps_lf.
	ps_ignore_line = 7,

	// Expected a LF and switches to ps_header_name.
	ps_lf = 8,

	// Reads the header's name. Switches either to ps_host if the header's
	// name is "Host" or to ps_ignore_line if it is not.
	ps_header_name_0 = 9,

	// Reads the Host header's value and finishes parsing.
	ps_host = 15,
};

enum parser_result {
	// The parser needs more data to make a decision.
	pr_continue,

	// The parser has encountered an error because the data is in an invalid
	// format. The parser should not be fed data again.
	pr_error,

	// The parsing has finished. The parser should not be fed data again.
	pr_finished,
};

// Advances the parsing using data from [data, data + len).
enum parser_result parser_go(uint8_t *req_parser_state, char *req_fields,
			     size_t req_fields_len, const char *data,
			     size_t len);

#endif
