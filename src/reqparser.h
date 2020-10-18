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

#ifndef HTTP2SD_REQPARSER_H
#define HTTP2SD_REQPARSER_H

#include <stddef.h>
#include <stdint.h>

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
