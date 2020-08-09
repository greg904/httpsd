#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

enum request_state
{
	// Expects the GET method
	request_method,

	// Reads the request URI
	request_uri,

	// Ignores everything until it encounters a CR in which case it switches
	// to request_lf
	request_ignore_line,

	// Expected a LF and switches to request_header_name
	request_lf,

	// Reads the header's name. Switches either to request_header_host if
	// the header's name is "Host" or to request_ignore_line if it is not.
	request_header_name,

	// Reads the Host header's value and responds to the client
	request_header_host,
};

void memcpy_increment_dest(char *restrict *dest, const char *restrict src, size_t n)
{
	memcpy(*dest, src, n);
	*dest += n;
}

void send_response(int client_fd, const char *request_uri_buf, uint32_t request_uri_len, const char *request_host_buf, uint32_t request_host_len)
{
	const char response_start[] = "HTTP/1.1 301 Moved Permanently\nLocation: https://";
	const char response_end[] = "\r\n\r\n";

	size_t response_len = (sizeof(response_start) - 1) + request_host_len + request_uri_len + (sizeof(response_end) - 1);
	char *response_buf = malloc(response_len);

	char *tmp = response_buf;
	memcpy_increment_dest(&tmp, response_start, sizeof(response_start) - 1);
	memcpy_increment_dest(&tmp, request_host_buf, request_host_len);
	memcpy_increment_dest(&tmp, request_uri_buf, request_uri_len);
	memcpy_increment_dest(&tmp, response_end, sizeof(response_end) - 1);

	if (write(client_fd, response_buf, response_len) == -1) {
		// This is to be expected if the request has timed out.
		if (errno != EINTR)
			perror("write()");

		return;
	}
}

void handle_client(int client_fd)
{
	enum request_state parser_state = request_method;
	uint32_t parser_tmp = 0; // the meaning depends on state

	char request_uri_buf[512];
	uint32_t request_uri_len = 0;

	char request_host_buf[512];
	uint32_t request_host_len = 0;

	for (;;) {
		char receive_buf[1024];
		ssize_t n = read(client_fd, receive_buf, sizeof(receive_buf));
		if (n == -1) {
			// This is to be expected if the request has timed out.
			if (errno != EINTR)
				perror("read()");
			
			close(client_fd);
			return;
		}
		if (n == 0) {
			fputs("EOS without parsing finished\n", stderr);
			close(client_fd);
			return;
		}

		ssize_t read_index = 0;

		do {
			switch (parser_state) {
			case request_method: {
				const char method_str[] = "GET ";

				for (;;) {
					if (receive_buf[read_index++] != method_str[parser_tmp++]) {
						fputs("invalid HTTP method\n", stderr);
						close(client_fd);
						return;
					}
					
					if (parser_tmp == sizeof(method_str) - 1) {
						parser_state = request_uri;
						parser_tmp = 0;
						break;
					}

					if (read_index >= n)
						break;
				}

				break;
			}
			case request_uri:
				for (;;) {
					char c = receive_buf[read_index++];
					if (c == ' ') {
						parser_state = request_ignore_line;
						parser_tmp = 0;
						break;
					}

					if (request_uri_len >= sizeof(request_uri_buf)) {
						fputs("request URI too long\n", stderr);
						close(client_fd);
						return;
					}

					request_uri_buf[request_uri_len] = c;
					request_uri_len++;

					if (read_index >= n)
						break;
				}

				break;
			case request_ignore_line:
				for (;;) {
					char c = receive_buf[read_index++];
					if (c == '\r') {
						parser_state = request_lf;
						parser_tmp = 0;
						break;
					}

					if (read_index >= n)
						break;
				}

				break;
			case request_lf:
				if (receive_buf[read_index++] != '\n') {
					fputs("expected LF but got something else\n", stderr);
					close(client_fd);
					return;
				}
				
				parser_state = request_header_name;
				parser_tmp = 0;

				break;
			case request_header_name: {
				const char host_str[] = "Host: ";

				for (;;) {
					if (receive_buf[read_index++] != host_str[parser_tmp++]) {
						parser_state = request_ignore_line;
						parser_tmp = 0;
						break;
					}
					
					if (parser_tmp == sizeof(host_str) - 1) {
						parser_state = request_header_host;
						parser_tmp = 0;
						break;
					}

					if (read_index >= n)
						break;
				}

				break;
			}
			case request_header_host:
				for (;;) {
					char c = receive_buf[read_index++];
					if (c == '\r') {
						send_response(client_fd, request_uri_buf, request_uri_len, request_host_buf, request_host_len);
						close(client_fd);
						return;
					}

					if (request_host_len >= sizeof(request_host_buf)) {
						fputs("request host too long\n", stderr);
						close(client_fd);
						return;
					}

					request_host_buf[request_host_len] = c;
					request_host_len++;

					if (read_index >= n)
						break;
				}

				break;
			}
		} while (read_index < n);
	}
}

void empty_signal_handler(int signal)
{
}

int main(int argc, char **argv)
{
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd == -1) {
		perror("socket()");
		return 1;
	}

	struct sockaddr_in addr = {};
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);

	if (bind(sock_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		perror("bind()");
		return 1;
	}

	if (listen(sock_fd, 4) == -1) {
		perror("listen()");
		return 1;
	}

	// Ignore the alarm signal because we only use the EINTR return value
	// returned by the systtem calls to implement request timeout.
	struct sigaction alarm_act = {};
	alarm_act.sa_handler = empty_signal_handler;
	if (sigaction(SIGALRM, &alarm_act, NULL) == -1) {
		perror("sigaction()");
		return 1;
	}

	for (;;) {
		struct sockaddr_in client_addr = {};
		socklen_t client_addr_len = sizeof(client_addr);

		int accept_fd = accept(sock_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		if (accept_fd == -1) {
			perror("accept()");
			return 1;
		}

		const char *parts = (const char *) &client_addr.sin_addr.s_addr;
		printf("connection from %d.%d.%d.%d\n", parts[0], parts[1], parts[2], parts[3]);
		
		alarm(1); // Start request timeout
		handle_client(accept_fd);
		alarm(0); // Stop request timeout
	}
}
