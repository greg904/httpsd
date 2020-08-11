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

#ifndef G9_HTTPS_REDIRECT_UTIL_H
#define G9_HTTPS_REDIRECT_UTIL_H

#define UNUSED(x) (void)x;

#define STR_VALUE(x) #x
#define STR_VALUE_MACRO(x) STR_VALUE(x)

/**
 * A temporary buffer used to read requests or write responses.
 */
extern char util_tmp_buf[512];

void util_reverse(char *start, char *end);

#endif
