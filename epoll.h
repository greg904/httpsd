#ifndef G9_HTTPS_REDIRECT_EPOLL_H
#define G9_HTTPS_REDIRECT_EPOLL_H

#include <stdbool.h>

extern int epoll_fd;

bool epoll_setup();
bool epoll_run();

bool epoll_on_server_in();
bool epoll_on_conn_in(int client_index);
bool epoll_on_conn_out(int client_index);
bool epoll_on_event();

#endif
