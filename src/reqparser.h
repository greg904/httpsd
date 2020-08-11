#ifndef G9_HTTPS_REDIRECT_PARSER_H
#define G9_HTTPS_REDIRECT_PARSER_H

#include <stdint.h>
#include <stdlib.h>

struct reqparser_args {
	uint8_t state;

	/* Input */
	const char *data;
	const char *data_end;

	/* Output */
	char *req_fields;
	size_t req_fields_len;
};

enum reqparser_completion {
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
	PC_BAD_DATA,

	/**
	 * There is not enough space in the buffer to store the request fields
	 * (path and host).
	 */
	PC_BUFFER_TOO_SMALL,
};

/**
 * Advances the HTTP request parsing.
 */
enum reqparser_completion reqparser_feed(struct reqparser_args *args);

#endif
