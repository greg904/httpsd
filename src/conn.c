#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "conn.h"
#include "reqparser.h"
#include "util.h"

/* If this becomes greater than 32, then the uint32_t type that is used
   throughout the code needs to be changed. */
#define MAX_CONN_COUNT 28

/**
 * Custom reqparser_state for RC_BUFFER_TOO_SMALL error, so that we don't need
 * another field in the conn struct.
 */
#define REQPARSER_CUSTOM_ERR 15

/**
 * Includes the state about a socket connection to the HTTP server socket. Note
 * that the size of this struct is precisely 256 bytes.
 */
struct conn {
	/**
	 * If we haven't received a valid request after the timeout, we will
	 * close the socket and free the client object.
	 * This is to prevent potential badly behaving clients that would open
	 * a connection to the server, not send anything (or not finish the
	 * request) and never close the connection from taking up space and
	 * preventing other good clients from connecting.
	 */
	uint64_t timeout;

	int socket_fd;

	/**
	 * The write syscall might not write the whole buffer but only a part of
	 * it, therefore we must keep track of how many bytes we have already
	 * sent in order to know what to send the next time we get a EPOLLIN
	 * event.
	 */
	uint16_t res_bytes_sent : 12;

	uint8_t reqparser_state : 4;

	/**
	 * At the end of the parsing, this will contain the request URI, then
	 * a NULL character, then the request host, then a NULL character or
	 * no character if it's the end of the array.
	 */
	char req_fields[242];
};

static struct conn connections[MAX_CONN_COUNT];

/**
 * For every connections info object that is currently valid (between a conn_new
 * and conn_free), a bit at the index of the ID is set in this bitmap.
 */
static uint32_t connections_bitmap;

static bool conn_is_valid(int id);

static size_t conn_write_redirect_response(int id, char *buf, size_t capacity);
static size_t conn_write_too_long_response(char *buf, size_t capacity);

int conn_new(int socket_fd)
{
	for (int id = 0; id < MAX_CONN_COUNT; id++) {
		/* Check if that index is already used. */
		if (conn_is_valid(id))
			continue;
		connections_bitmap |= (1 << id);
		connections[id].socket_fd = socket_fd;
		return id;
	}
	return -1;
}

void conn_free(int index)
{
	connections_bitmap &= ~(1 << index);

	/* Reset the fields for later, if the index gets reused. */
	struct conn *c = &connections[index];
	c->res_bytes_sent = 0;
	c->reqparser_state = 0;
	memset(c->req_fields, 0, sizeof(c->req_fields));
}

void conn_for_each(void (*cb)(int))
{
	for (int id = 0; id < MAX_CONN_COUNT; id++) {
		if (conn_is_valid(id))
			cb(id);
	}
}

int conn_get_socket_fd(int id) { return connections[id].socket_fd; }

void conn_set_timeout(int id, uint64_t timeout)
{
	connections[id].timeout = timeout;
}

uint64_t conn_get_timeout(int id) { return connections[id].timeout; }

enum conn_wants_more conn_recv(int id, const char *data, size_t len)
{
	assert(conn_is_valid(id));
	struct conn *c = &connections[id];

	struct reqparser_args args;
	args.state = c->reqparser_state;
	args.data = data;
	args.data_end = data + len;
	args.req_fields = c->req_fields;
	args.req_fields_len = sizeof(c->req_fields);

	enum reqparser_completion result = reqparser_feed(&args);
	switch (result) {
	case PC_COMPLETE:
		return CWM_NO;
	case PC_NEEDS_MORE_DATA:
		c->reqparser_state = args.state;
		return CWM_YES;
	case PC_BAD_DATA:
		return CWM_ERROR;
	case PC_BUFFER_TOO_SMALL:
		c->reqparser_state = REQPARSER_CUSTOM_ERR;
		return CWM_NO;
	}
}

enum conn_wants_more conn_send(int id)
{
	assert(conn_is_valid(id));
	struct conn *c = &connections[id];

	/* We can afford to rebuild the whole response on every EPOLLIN
	   notification because there should only be 1 for a given socket most
	   of the time so in reality, we're only going to do this once. */
	size_t total_response_len =
	    c->reqparser_state == REQPARSER_CUSTOM_ERR
		? conn_write_too_long_response(util_tmp_buf,
					       sizeof(util_tmp_buf))
		: conn_write_redirect_response(id, util_tmp_buf,
					       sizeof(util_tmp_buf));

	for (;;) {
		size_t remaining = total_response_len - c->res_bytes_sent;
		if (remaining == 0)
			return CWM_NO;

		ssize_t written = write(
		    c->socket_fd, util_tmp_buf + c->res_bytes_sent, remaining);
		if (written == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return CWM_YES;

			perror("write()");
			return CWM_ERROR;
		}
		c->res_bytes_sent += written;
	}
}

static bool conn_is_valid(int id)
{
	return (connections_bitmap & (1 << id)) != 0;
}

static size_t conn_write_redirect_response(int id, char *buf, size_t capacity)
{
	assert(conn_is_valid(id));
	struct conn *c = &connections[id];
	char *cursor = buf;

	const char header[] =
	    "HTTP/1.1 301 Moved Permanently\r\nLocation: https://";
	size_t header_len = sizeof(header) - 1;
	assert((cursor + header_len) - buf <= (ptrdiff_t)capacity);
	memcpy(cursor, header, header_len);
	cursor += header_len;

	/* Find the index of the NULL character that delimits the request URL
	   path from the request host. */
	size_t sep_index = strlen(c->req_fields);

	char *host_start = c->req_fields + sep_index + 1;
	char *host_end = host_start;
	while (*host_end != '\0' &&
	       host_end != c->req_fields + sizeof(c->req_fields))
		host_end++;
	size_t host_len = host_end - host_start;

	/* URL host */
	assert((cursor + host_len) - buf <= (ptrdiff_t)capacity);
	memcpy(cursor, c->req_fields + sep_index + 1, host_len);
	cursor += host_len;

	/* URL slash */
	assert((cursor + 1) - buf <= (ptrdiff_t)capacity);
	*cursor = '/';
	cursor++;

	/* URL path */
	assert((cursor + sep_index) - buf <= (ptrdiff_t)capacity);
	memcpy(cursor, c->req_fields, sep_index);
	cursor += sep_index;

	const char footer[] =
	    "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
	size_t footer_len = sizeof(footer) - 1;
	assert((cursor + footer_len) - buf <= (ptrdiff_t)capacity);
	memcpy(cursor, footer, footer_len);
	cursor += footer_len;

	return cursor - buf;
}

static size_t conn_write_too_long_response(char *buf, size_t capacity)
{
	const char body[] =
	    "HTTP/1.1 414 URI Too Long\r\nContent-Length: "
	    "45\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nThe "
	    "combined URL host and path is too large!\n";
	size_t body_len = sizeof(body) - 1;
	assert(body_len <= capacity);
	memcpy(buf, body, body_len);
	return body_len;
}
