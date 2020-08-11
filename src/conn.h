#ifndef G9_HTTPS_REDIRECT_CONN_H
#define G9_HTTPS_REDIRECT_CONN_H

#include <stddef.h>
#include <stdint.h>

/**
 * Creates a new connection info object to accompany a socket connection and
 * returns its ID or -1 if there is no more space available.
 */
int conn_new(int socket_fd);

/**
 * When the code is done with a connection, this method should be called with
 * the accompanying connection info object's ID in order to free the space for
 * new connections.
 */
void conn_free(int id);

/**
 * Call the function given as an argument with the IDs of active connections
 * info objects (once per connection).
 */
void conn_for_each(void (*cb)(int));

int conn_get_socket_fd(int id);

void conn_set_timeout(int id, uint64_t timeout);
uint64_t conn_get_timeout(int id);

enum conn_wants_more {
	CWM_YES,
	CWM_NO,
	CWM_ERROR,
};

/**
 * Parses a new chunk of data that has been received. This should only be called
 * during the read phase of a connection.
 */
enum conn_wants_more conn_recv(int id, const char *data, size_t len);

/**
 * Tries to send a chunk of data to the socket, because epoll has been notified
 * that the socket is writable. This should only be called during the write
 * phase of a connection.
 */
enum conn_wants_more conn_send(int id);

#endif
