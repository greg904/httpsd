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

bool util_fputs(int fd, const char *str, size_t len)
{
	return sys_write(fd, str, len) == (ssize_t)len;
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
