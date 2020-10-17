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

#include "util.h"

char util_tmp_buf[512];

bool _util_fputs(int fd, const char *str, size_t len)
{
	return sys_write(fd, str, len) == len;
}

void util_reverse(char *start, char *end)
{
	ASSERT(end >= start);

	while (start < end) {
		/* Swap bytes. */
		char tmp = *start;
		*start = *end;
		*end = tmp;

		start++;
		end--;
	}
}

size_t util_strlen(const char *str)
{
	const char *start = str;
	while (*str != '\0')
		str++;
	return str - start;
}

bool util_strcmp(const char *a, const char *b)
{
	for (; *a != '\0'; ++a, ++b) {
		if (*a != *b)
			return false;
	}

	if (*b != '\0')
		return false;

	return true;
}

uint16_t util_htob16(uint16_t val) { return ((val & 0xFF) << 8) | (val >> 8); }

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	char *dest_c = (char *)dest;
	const char *src_c = (const char *)src;

	ASSERT(dest_c <= src_c || dest_c >= src_c + n);

	for (; n != 0; --n, ++src_c, ++dest_c)
		*dest_c = *src_c;

	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	char *dest_c = (char *)dest;
	const char *src_c = (const char *)src;

	/* My memcpy implementation handles that case correctly already. */
	if (dest_c <= src_c || dest_c >= src_c + n)
		return memcpy(dest, src, n);

	/* Otherwise, do it in reverse. */
	src_c += n;
	dest_c += n;
	for (; n != 0; --n, --src_c, --dest_c)
		*dest_c = *src_c;

	return dest;
}

void *memset(void *dest, int c, size_t n)
{
	char *dest_c = (char *)dest;
	for (; n != 0; --n, ++dest_c)
		*dest_c = c;
	return dest;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const char *a_c = (const char *)a;
	const char *b_c = (const char *)b;

	for (; n != 0; --n, ++a_c, ++b_c) {
		if (*a_c < *b_c) {
			return -1;
		} else if (*a_c > *b_c) {
			return 1;
		}
	}

	return 0;
}
