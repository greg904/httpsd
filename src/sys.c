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
#include "util.h"

#ifndef HTTP2SD_NO_VDSO
#	include "../libs/parse_vdso.h"

typedef int (*sys_clock_gettime_t)(clockid_t, struct timespec *);
static sys_clock_gettime_t sys_clock_gettime_vdso;
#endif

extern void main(char **argv);

#ifdef __x86_64__
/* The actual entry point that calls main. */
asm(".global _start\n"
    "_start:\n"
    "xorl %ebp, %ebp\n" /* This is the outermost stack frame. */
    "movq %rsp, %rdi\n" /* Put argv in first argument. */
    "call sys_main_amd64\n");

void sys_main_amd64(void *stack)
{
	char *stack_c = (char *)stack;
	char **argv = (char **)(stack_c + 8);

#	ifndef HTTP2SD_NO_VDSO
	uint64_t argc = *(uint64_t *)stack_c;
	char **envp = argv + argc + 1;

	/* Find the start of the auxiliary vector. */
	char **auxv = envp;
	while (*auxv != NULL)
		++auxv;
	++auxv;

	vdso_init_from_auxv((void *)auxv);
	sys_clock_gettime_vdso =
	    (sys_clock_gettime_t)vdso_sym("LINUX_2.6", "__vdso_clock_gettime");
#	endif

	main(argv);
}

static uint64_t sys_1(uint64_t num, uint64_t a);
static uint64_t sys_2(uint64_t num, uint64_t a, uint64_t b);
static uint64_t sys_3(uint64_t num, uint64_t a, uint64_t b, uint64_t c);
static uint64_t sys_4(uint64_t num, uint64_t a, uint64_t b, uint64_t c,
		      uint64_t d);
#else
#	error Only the amd64 architecture is supported for now!
#endif

ssize_t sys_read(int fd, void *buf, size_t count)
{
	return (ssize_t)sys_3(0, fd, (uint64_t)buf, count);
}

ssize_t sys_write(int fd, const void *buf, size_t count)
{
	return (ssize_t)sys_3(1, fd, (uint64_t)buf, count);
}

int sys_close(int fd) { return (int)sys_1(3, fd); }

noreturn void sys_exit(int code)
{
	sys_1(60, code);
	for (;;) {
	}
}

int sys_socket(int family, int type, int protocol)
{
	return (int)sys_3(41, family, type, protocol);
}

int sys_bind(int fd, struct sockaddr *addr, size_t addr_len)
{
	return (int)sys_3(49, fd, (uint64_t)addr, addr_len);
}

int sys_listen(int fd, int backlog) { return (int)sys_2(50, fd, backlog); }

int sys_accept4(int fd, struct sockaddr *peer_addr, int *peer_addr_len,
		int flags)
{
	return (int)sys_4(288, fd, (uint64_t)peer_addr, (uint64_t)peer_addr_len,
			  flags);
}

int sys_epoll_create1(int flags) { return (int)sys_1(291, flags); }

int sys_epoll_ctl(int epoll_fd, int op, int fd, struct epoll_event *event)
{
	return (int)sys_4(233, epoll_fd, op, fd, (uint64_t)event);
}

int sys_epoll_wait(int epoll_fd, struct epoll_event *events, int max_events,
		   int timeout)
{
	return (int)sys_4(232, epoll_fd, (uint64_t)events, max_events, timeout);
}

int sys_clock_gettime(clockid_t clock, struct timespec *time)
{
#ifndef HTTP2SD_NO_VDSO
	if (sys_clock_gettime_vdso != NULL)
		return sys_clock_gettime_vdso(clock, time);
#endif

	return (int)sys_2(228, clock, (uint64_t)time);
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	char *dest_c = (char *)dest;
	const char *src_c = (const char *)src;

	ASSERT(dest_c <= src_c || dest_c >= src_c + n);

	for (; n != 0; --n, ++src_c, ++dest_c)
		*dest_c = *src_c;

	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	char *dest_c = (char *)dest;
	const char *src_c = (const char *)src;

	/* My memcpy implementation handles that case correctly already. */
	if (dest_c <= src_c || dest_c >= src_c + n)
		return memcpy(dest, src, n);

	/* Otherwise, do it in reverse. */
	src_c += n;
	dest_c += n;
	for (; n != 0; --n, --src_c, --dest_c)
		*dest_c = *src_c;

	return dest;
}

void *memset(void *dest, int c, size_t n)
{
	char *dest_c = (char *)dest;
	for (; n != 0; --n, ++dest_c)
		*dest_c = c;
	return dest;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const char *a_c = (const char *)a;
	const char *b_c = (const char *)b;

	for (; n != 0; --n, ++a_c, ++b_c) {
		if (*a_c < *b_c) {
			return -1;
		} else if (*a_c > *b_c) {
			return 1;
		}
	}

	return 0;
}

size_t strlen(const char *str)
{
	const char *start = str;
	while (*str != '\0')
		str++;
	return str - start;
}

int strcmp(const char *a, const char *b)
{
	for (; *a != '\0'; ++a, ++b) {
		if (*b == '\0')
			return 1;

		if (*a < *b) {
			return -1;
		} else if (*a > *b) {
			return 1;
		}
	}

	if (*b != '\0')
		return -1;

	return 0;
}

uint16_t htons(uint16_t val)
{
	uint8_t *val_c = (uint8_t *)&val;
	return (uint16_t)(val_c[0] << 8) | (uint16_t)val_c[1];
}

#ifdef __x86_64__
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
#else
#	error Only the amd64 architecture is supported for now!
#endif
