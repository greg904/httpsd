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

#include "epoll.h"
#include "misc.h"

#ifndef SERVER_PORT
#	define SERVER_PORT 8080
#endif

char tmp_buf[512];

int main(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	int server_fd =
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

	if (!epoll_init(server_fd))
		return EXIT_FAILURE;

	if (listen(server_fd, 4) == -1) {
		perror("listen()");
		return EXIT_FAILURE;
	}

	for (;;) {
		if (!epoll_wait_and_dispatch())
			return EXIT_FAILURE;
	}
}

