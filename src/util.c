#include "util.h"

char util_tmp_buf[512];

void util_reverse(char *start, char *end)
{
	while (start < end) {
		/* Swap bytes. */
		char tmp = *start;
		*start = *end;
		*end = tmp;

		start++;
		end--;
	}
}
