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

#ifndef HTTP2SD_UTIL_H
#define HTTP2SD_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sys.h"

#define UNUSED(x) (void)x;

#define STR_VALUE_(x) #x
#define STR_VALUE(x) STR_VALUE_(x)
#define STR_LINE STR_VALUE(__LINE__)

#define FPUTS_0(fd, str) util_fputs(fd, str, strlen(str))
#define FPUTS_A(fd, str) util_fputs(fd, str, sizeof(str) - 1)

#define ASSERT(check)                                                          \
	if (!(check)) {                                                        \
		FPUTS_A(2, "Assertion failed at ") && FPUTS_A(2, __FILE__) &&  \
		    FPUTS_A(2, ":" STR_LINE ".\n");                            \
		sys_exit(1);                                                   \
	}

#define ASSERT_UNREACHABLE()                                                   \
	do {                                                                   \
		FPUTS_A(2, "Code that was not supposed to be reachable was "   \
			   "executed at ") &&                                  \
		    FPUTS_A(2, __FILE__) && FPUTS_A(2, ":" STR_LINE ".\n");    \
		sys_exit(1);                                                   \
	} while (false)

/**
 * A temporary buffer used to read requests or write responses.
 */
extern char util_tmp_buf[512];

/**
 * This function is used by the FPUTS_0 and FPUTS_A macros.
 */
bool util_fputs(int fd, const char *str, size_t len);

void util_reverse(char *start, char *end);

#endif
