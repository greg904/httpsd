# Make sure that the shell is the same everywhere.
SHELL = /bin/sh

src_c := $(wildcard src/*.c)
objs := $(src_c:%.c=%.o)

CFLAGS = -std=gnu11 -ffreestanding -nostdlib -flto -fPIC -O2 -Wall -Wextra -Werror
LDLIBS = -lflibc
LDFLAGS = -static

INSTALL = install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

.PHONY: all
all: http2sd

.PHONY: clean
clean:
	rm -f $(objs) gstatus

.PHONY: format
format:
	clang-format -i $(src_c) include/flibc/*.h

###
# Compilation
###

%.o: %.c
	$(CC) $< -c -MD -o $@ -Iflibc/include $(CPPFLAGS) $(CFLAGS)

flibc/libflibc.a:
	$(MAKE) -C flibc

http2sd: $(objs) flibc/libflibc.a
	$(CC) $(objs) -o $@ -Lflibc $(CFLAGS) $(LDLIBS) $(LDFLAGS)

###
# Installation
###

.PHONY: install
install: http2sd http2sd.service
	$(INSTALL_PROGRAM) http2sd $(DESTDIR)/usr/bin/http2sd
	$(INSTALL_DATA) http2sd.service $(DESTDIR)/usr/lib/systemd/system/http2sd.service
