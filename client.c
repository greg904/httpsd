#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "parser.h"
#include "util.h"

// If this becomes greater than 16, then the uint16_t type that is used
// throughout the code needs to be changed.
#define MAX_CLIENTS 16

static struct client clients[MAX_CLIENTS];

// For every clients object that is currently used, a bit is set in this bitmap.
static uint16_t clients_bitmap;

int client_allocate()
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		uint16_t mask = 1 << i;
		// Check if that index is already used
		if ((clients_bitmap & mask) == 0) {
			clients_bitmap |= mask;
			return i;
		}
	}
	return -1;
}

void client_free(int index)
{
	clients_bitmap &= ~(1 << index);

	// Reset the fields for later
	struct client *c = &clients[index];
	c->res_bytes_sent = 0;
	c->req_parser_state = ps_method_0;
	memset(c->req_fields, 0, sizeof(c->req_fields));
}

struct client *client_get(int index) {
	if ((clients_bitmap & (1 << index)) == 0)
		return NULL;
	return &clients[index];
}

int client_get_max_index() {
	return MAX_CLIENTS;
}

size_t client_write_response(struct client *c, char *response)
{
	char *tmp = response;

	const char response_start[] =
	    "HTTP/1.1 301 Moved Permanently\r\nLocation: https://";
	memcpy(response, response_start, sizeof(response_start) - 1);
	tmp += sizeof(response_start) - 1;

	size_t sep_index = strlen(c->req_fields);

	char *host_start = c->req_fields + sep_index + 1;
	char *host_end = host_start;
	while (*host_end != '\0' && host_end != c->req_fields + sizeof(c->req_fields))
		host_end++;
	size_t host_len = host_end - host_start;

	// Host
	memcpy(tmp, c->req_fields + sep_index + 1, host_len);
	tmp += host_len;

	// Slash
	*tmp = '/';
	tmp++;

	// URI
	memcpy(tmp, c->req_fields, sep_index);
	tmp += sep_index;

	const char response_end[] = "\r\n\r\n";
	memcpy(tmp, response_end, sizeof(response_end) - 1);
	tmp += sizeof(response_end) - 1;

	return tmp - response;
}

enum client_respond client_continue_sending_response(struct client *c)
{
	size_t res_len = client_write_response(c, tmp_buf);

	for (;;) {
		size_t remaining = res_len - c->res_bytes_sent;
		if (remaining == 0)
			return cr_finished;

		ssize_t written =
		    write(c->socket_fd, tmp_buf + c->res_bytes_sent, remaining);
		if (written == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return cr_continue;

			perror("write()");
			return cr_error;
		}
		c->res_bytes_sent += written;
	}
}
