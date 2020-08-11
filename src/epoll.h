#ifndef G9_HTTPS_REDIRECT_EPOLL_H
#define G9_HTTPS_REDIRECT_EPOLL_H

#include <stdbool.h>

/**
 * Initializes the epoll module. Takes the HTTP server socket's FD as an
 * argument.
 */
bool epoll_init(int server_socket_fd);

/**
 * Blocks until something is worth doing and does it.
 */
bool epoll_wait_and_dispatch();

#endif
