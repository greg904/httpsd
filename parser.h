#ifndef G9_HTTPS_REDIRECT_PARSER_H
#define G9_HTTPS_REDIRECT_PARSER_H

#include <stdint.h>
#include <stdlib.h>

struct parse_args {
	uint8_t state;

	/* Input */
	const char *data;
	const char *data_end;

	/* Output */
	char *req_fields;
	size_t req_fields_len;
};

enum parse_completion {
	/**
	 * The parsing has finished. The parser should not be fed data again.
	 */
	PC_COMPLETE,

	/**
	 * The parser needs more data to make a decision.
	 */
	PC_NEEDS_MORE_DATA,

	/**
	 * The parser has encountered an error because the data is in an invalid
	 * format. The parser should not be fed data again.
	 */
	PC_ERROR,
};

/**
 * Advances the HTTP request parsing.
 */
enum parse_completion parser_go(struct parse_args *args);

#endif
