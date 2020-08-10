#ifndef G9_HTTPS_REDIRECT_PARSER_H
#define G9_HTTPS_REDIRECT_PARSER_H

#include <stdint.h>
#include <stdlib.h>

enum parser_state {
	// Expects the GET method.
	ps_method,

	// Reads the request URI.
	ps_uri,

	// Ignores everything until it encounters a CR in which case it switches
	// to ps_lf.
	ps_ignore_line,

	// Expected a LF and switches to ps_header_name.
	ps_lf,

	// Reads the header's name. Switches either to ps_host if the header's
        // name is "Host" or to ps_ignore_line if it is not.
	ps_header_name,

	// Reads the Host header's value and finishes parsing.
	ps_host,
};

struct parser {
        enum parser_state state;

	// This is for remembering where we are in the HTTP method or in a HTTP
	// header name if it gets split between two calls to read().
	uint8_t tmp;

	char uri[256];
	uint32_t uri_len;

	char host[256];
	uint32_t host_len;
};

enum parser_result {
	// The parser needs more data to make a decision.
	pr_continue,

	// The parsing has finished. The parser should not be fed data again.
	pr_finished,

        // The parser has encountered an error because the data is in an invalid
        // format. The parser should not be fed data again.
	pr_error,
};

void parser_reset(struct parser *p);

enum parser_result parser_feed(struct parser *p, const char *data, size_t len);

#endif
