#ifndef G9_HTTPS_REDIRECT_CLIENT_H
#define G9_HTTPS_REDIRECT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "config.h"

// Represents a socket connection to the server socket. Note that the size of
// this struct is precisely 256 bytes.
struct client {
	// If we haven't received a valid request after the timeout, we will
	// close the socket and free the client object.
	// This is to prevent potential badly behaving clients that would open
	// a connection to the server, not send anything (or not finish the
	// request) and never close the connection from taking up space and
	// preventing other good clients from connecting.
	uint64_t timeout;

	int socket_fd;

	// The write syscall might not write the whole buffer but only a part of
	// it, therefore we must keep track of how many bytes we have already
	// sent in order to know what to send the next time we get a EPOLLIN
	// event.
	uint16_t res_bytes_sent : 12;

	uint8_t req_parser_state : 4;

	// At the end of the parsing, this will contain the request URI, then
	// a NULL character, then the request host, then a NULL character or
	// no character if it's the end of the array.
	char req_fields[242];
};

enum client_respond {
	// There is more data to send, the method must be called again.
	cr_continue,

	cr_error,

	// We finished sending the response to the client. The socket can be
	// closed.
	cr_finished,
};

int client_allocate();
void client_free(int index);

struct client *client_get(int index);
int client_get_max_index();

size_t client_write_response(struct client *c, char *response);
enum client_respond client_continue_sending_response(struct client *c);

#endif
