#ifndef G9_HTTPS_REDIRECT_UTIL_H
#define G9_HTTPS_REDIRECT_UTIL_H

#define UNUSED(x) (void)x;

// A temporary buffer used to read requests or write responses.
extern char reuse_tmp_buf[512];

void util_reverse(char *start, char *end);

#endif
