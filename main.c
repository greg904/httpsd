#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "client.h"
#include "epoll.h"
#include "misc.h"
#include "parser.h"

int server_fd;
char tmp_buf[512];

int main(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1) {
		perror("epoll_create()");
		return EXIT_FAILURE;
	}

	server_fd =
	    socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (server_fd == -1) {
		perror("socket()");
		return EXIT_FAILURE;
	}

	struct sockaddr_in addr = { 0 };
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind()");
		return EXIT_FAILURE;
	}

	struct epoll_event server_epoll_event;
	server_epoll_event.data.u64 = 0;
	// We want to be notified when the server socket is ready to accept a
	// client socket.
	server_epoll_event.events = EPOLLIN | EPOLLET | EPOLLWAKEUP;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd,
		      &server_epoll_event) == -1) {
		perror("epoll_ctl()");
		return EXIT_FAILURE;
	}

	if (listen(server_fd, 4) == -1) {
		perror("listen()");
		return EXIT_FAILURE;
	}

	return epoll_run() ? EXIT_SUCCESS : EXIT_FAILURE;
}

