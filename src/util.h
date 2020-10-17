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

#define STR_VALUE(x) #x
#define STR_VALUE_MACRO(x) STR_VALUE(x)

#define FPUTS_0(fd, str) _fputs(fd, str, util_strlen(str))
#define FPUTS_A(fd, str) _fputs(fd, str, sizeof(str) - 1)

/**
 * A temporary buffer used to read requests or write responses.
 */
extern char util_tmp_buf[512];

/**
 * This function is used by the FPUTS_0 and FPUTS_A macros.
 */
bool _fputs(int fd, const char *str, size_t len);

void util_reverse(char *start, char *end);

size_t util_strlen(const char *str);
bool util_strcmp(const char *a, const char *b);

uint16_t util_htob16(uint16_t val);

/**
 * The following functions are not prefixed with util_ because they need to be
 * defined when using the freestanding environment with GCC.
 */

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);

#endif
