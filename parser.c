#include "parser.h"

void parser_reset(struct parser *p) {
        p->state = ps_method;
        p->tmp = 0;
        p->uri_len = 0;
        p->host_len = 0;
}

static enum parser_result parser_do_method(struct parser *p, const char **data, const char *data_end);
static enum parser_result parser_do_uri(struct parser *p, const char **data, const char *data_end);
static enum parser_result parser_do_ignore_line(struct parser *p, const char **data, const char *data_end);
static enum parser_result parser_do_lf(struct parser *p, const char **data, const char *data_end);
static enum parser_result parser_do_header_name(struct parser *p, const char **data, const char *data_end);
static enum parser_result parser_do_host(struct parser *p, const char **data, const char *data_end);

enum parser_result parser_feed(struct parser *p, const char *data, size_t len) {
        size_t read_index = 0;

        const char *data_end = data + len;

	do {
                enum parser_result result;

		switch (p->state) {
		case ps_method:
                        result = parser_do_method(p, &data, data_end);
                        break;
		case ps_uri:
                        result = parser_do_uri(p, &data, data_end);
			break;
		case ps_ignore_line:
			result = parser_do_ignore_line(p, &data, data_end);
			break;
		case ps_lf:
			result = parser_do_lf(p, &data, data_end);
			break;
		case ps_header_name:
                        result = parser_do_header_name(p, &data, data_end);
                        break;
		case ps_host:
                        result = parser_do_host(p, &data, data_end);
			break;
		}

                if (result != pr_continue)
                        return result;
	} while (read_index < len);

	// Everything parsed successfully.
	return pr_continue;
}

static enum parser_result parser_do_method(struct parser *p, const char **data, const char *data_end) {
        const char method_str[] = "GET ";

        do {
                if (*(*data)++ != method_str[p->tmp++]) {
                        // Invalid HTTP method
                        return pr_error;
                }

                if (p->tmp == sizeof(method_str) - 1) {
                        p->state = ps_uri;
                        p->tmp = 0;
                        break;
                }
        } while (*data != data_end);

        return pr_continue;
}

static enum parser_result parser_do_uri(struct parser *p, const char **data, const char *data_end) {
        do {
                char ch = *(*data)++;
                if (ch == ' ') {
                        p->state = ps_ignore_line;
                        break;
                }

                if (p->uri_len >= sizeof(p->uri)) {
                        // The request URI is too long
                        return pr_error;
                }

                p->uri[p->uri_len] = ch;
                p->uri_len++;
        } while (*data != data_end);

        return pr_continue;
}

static enum parser_result parser_do_ignore_line(struct parser *p, const char **data, const char *data_end) {
        do {
                char ch = *(*data)++;
                if (ch == '\r') {
                        p->state = ps_lf;
                        break;
                }
        } while (*data != data_end);

        return pr_continue;
}

static enum parser_result parser_do_lf(struct parser *p, const char **data, const char *data_end) {
        if (*(*data)++ != '\n') {
                // Expected a LF, but got something else
                return pr_error;
        }

        p->state = ps_header_name;

        return pr_continue;
}

static enum parser_result parser_do_header_name(struct parser *p, const char **data, const char *data_end) {
        const char host_str[] = "Host: ";

        do {
                if (*(*data)++ != host_str[p->tmp++]) {
                        p->state = ps_ignore_line;
                        p->tmp = 0;
                        break;
                }

                if (p->tmp == sizeof(host_str) - 1) {
                        p->state = ps_host;
                        p->tmp = 0;
                        break;
                }
        } while (*data != data_end);

        return pr_continue;
}

static enum parser_result parser_do_host(struct parser *p, const char **data, const char *data_end) {
        do {
                char ch = *(*data)++;
                if (ch == '\r')
                        return pr_finished;

                if (p->host_len >= sizeof(p->host)) {
                        // The request host is too long
                        return pr_error;
                }

                p->host[p->host_len] = ch;
                p->host_len++;
        } while (*data != data_end);

        return pr_continue;
}
