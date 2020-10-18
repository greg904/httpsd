object_files := src/cli.o \
	src/conn.o \
	src/epoll.o \
	src/main.o \
	src/reqparser.o \
	src/tmp.o
deps_files := $(object_files:%.o=%.d)

CCFLAGS ?= -fno-stack-protector -flto -fPIC -O2 -Wall -Wextra -Werror
CCFLAGS += -Iflibc/include -std=gnu11 -ffreestanding -nostdlib

LDFLAGS ?= ${CCFLAGS} -static
LDFLAGS += -Lflibc -lflibc

.PHONY: all
all: http2sd

.PHONY: clean
clean:
	rm -f ${object_files} ${deps_files} http2sd

.PHONY: format
format:
	clang-format -i src/*.c src/*.h

%.o: %.c
	${CC} $< -c -MD -o $@ ${CCFLAGS}

# The compiler will generate dependencies for each implementation file.
-include ${deps_files}

flibc/libflibc.a:
	${MAKE} -C flibc

http2sd: ${object_files} flibc/libflibc.a
	${CC} ${object_files} -o $@ ${LDFLAGS}
