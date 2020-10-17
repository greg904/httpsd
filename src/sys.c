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

#include "sys.h"

/* The actual entry point that calls main. */
asm(".global _start\n"
    "_start:\n"
    "xorl %ebp, %ebp\n"	  /* This is the outermost stack frame. */
    "lea 8(%rsp), %rdi\n" /* Put argv in first argument. */
    "call main\n");

static uint64_t sys_1(uint64_t num, uint64_t a);
static uint64_t sys_2(uint64_t num, uint64_t a, uint64_t b);
static uint64_t sys_3(uint64_t num, uint64_t a, uint64_t b, uint64_t c);
static uint64_t sys_4(uint64_t num, uint64_t a, uint64_t b, uint64_t c,
		      uint64_t d);

sys_ssize_t sys_read(int fd, void *buf, size_t count)
{
	return (sys_ssize_t)sys_3(0, fd, (uint64_t)buf, count);
}

sys_ssize_t sys_write(int fd, const void *buf, size_t count)
{
	return (sys_ssize_t)sys_3(1, fd, (uint64_t)buf, count);
}

int sys_close(int fd) { return (int)sys_1(3, fd); }

__attribute__((noreturn)) void sys_exit(int code)
{
	sys_1(60, code);
	for (;;) {
	}
}

int sys_socket(int family, int type, int protocol)
{
	return (int)sys_3(41, family, type, protocol);
}

int sys_bind(int fd, struct sys_sockaddr *addr, size_t addr_len)
{
	return (int)sys_3(49, fd, (uint64_t)addr, addr_len);
}

int sys_listen(int fd, int backlog) { return (int)sys_2(50, fd, backlog); }

int sys_accept4(int fd, struct sys_sockaddr *peer_addr, int *peer_addr_len,
		int flags)
{
	return (int)sys_4(288, fd, (uint64_t)peer_addr, (uint64_t)peer_addr_len,
			  flags);
}

int sys_epoll_create1(int flags) { return (int)sys_1(291, flags); }

int sys_epoll_ctl(int epoll_fd, int op, int fd, struct sys_epoll_event *event)
{
	return (int)sys_4(233, epoll_fd, op, fd, (uint64_t)event);
}

int sys_epoll_wait(int epoll_fd, struct sys_epoll_event *events, int max_events,
		   int timeout)
{
	return (int)sys_4(232, epoll_fd, (uint64_t)events, max_events, timeout);
}

int sys_clock_gettime(sys_clockid_t clock, struct sys_timespec *time)
{
	return (int)sys_2(228, clock, (uint64_t)time);
}

static uint64_t sys_1(uint64_t num, uint64_t a)
{
	uint64_t ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "0"(num), "D"(a)
		     : "rcx", "r11", "memory");
	return ret;
}

static uint64_t sys_2(uint64_t num, uint64_t a, uint64_t b)
{
	uint64_t ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "0"(num), "D"(a), "S"(b)
		     : "rcx", "r11", "memory");
	return ret;
}

static uint64_t sys_3(uint64_t num, uint64_t a, uint64_t b, uint64_t c)
{

	uint64_t ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "0"(num), "D"(a), "S"(b), "d"(c)
		     : "rcx", "r11", "memory");
	return ret;
}

static uint64_t sys_4(uint64_t num, uint64_t a, uint64_t b, uint64_t c,
		      uint64_t d)
{
	register long r10 asm("r10") = d;

	uint64_t ret;
	asm volatile("syscall"
		     : "=a"(ret)
		     : "0"(num), "D"(a), "S"(b), "d"(c), "r"(r10)
		     : "rcx", "r11", "memory");
	return ret;
}
