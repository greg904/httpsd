/*
 * Copyright (C) 2020 Greg Depoire--Ferrer <greg.depoire@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HTTP2SD_SYS_H
#define HTTP2SD_SYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYS_EAGAIN 11

#define SYS_AF_INET 2
#define SYS_INADDR_ANY 0x00000000

#define SYS_SOCK_CLOEXEC SYS_O_CLOEXEC
#define SYS_SOCK_NONBLOCK SYS_O_NONBLOCK

#define SYS_O_CLOEXEC 02000000
#define SYS_O_NONBLOCK 00004000

#define SYS_EPOLL_CLOEXEC SYS_O_CLOEXEC

#define SYS_EPOLL_CTL_ADD 1
#define SYS_EPOLL_CTL_DEL 2
#define SYS_EPOLL_CTL_MOD 3

#define SYS_EPOLLIN 0x00000001
#define SYS_EPOLLOUT 0x00000004
#define SYS_EPOLLERR 0x00000008
#define SYS_EPOLLRDHUP 0x00002000
#define SYS_EPOLLET (1U << 31)
#define SYS_EPOLLWAKEUP (1U << 29)

#define SYS_CLOCK_MONOTONIC 1

typedef int64_t sys_ssize_t;

enum sys_socktype {
	SYS_SOCK_STREAM = 1,
};

struct sys_sockaddr {
	unsigned short sa_family;
	char sa_data[14];
};

struct sys_sockaddr_in {
	unsigned short sin_family;
	uint16_t sin_port;
	uint32_t sin_addr;
	char padding[8];
};

typedef union sys_epoll_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} sys_epoll_data_t;

struct sys_epoll_event {
	int events;
	sys_epoll_data_t data;
};

typedef int sys_clockid_t;

struct sys_timespec {
	long tv_sec;
	long tv_nsec;
};

sys_ssize_t sys_read(int fd, void *buf, size_t count);
sys_ssize_t sys_write(int fd, const void *buf, size_t count);
int sys_close(int fd);
__attribute__((noreturn)) void sys_exit(int code);

int sys_socket(int family, int type, int protocol);
int sys_bind(int fd, struct sys_sockaddr *addr, size_t addr_len);
int sys_listen(int fd, int backlog);
int sys_accept4(int fd, struct sys_sockaddr *peer_addr, int *peer_addr_len,
		int flags);

int sys_epoll_create1(int flags);
int sys_epoll_ctl(int epoll_fd, int op, int fd, struct sys_epoll_event *event);
int sys_epoll_wait(int epoll_fd, struct sys_epoll_event *events, int max_events,
		   int timeout);

int sys_clock_gettime(sys_clockid_t clock, struct sys_timespec *time);

#endif
