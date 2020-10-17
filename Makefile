object_files := src/cli.o \
	src/conn.o \
	src/epoll.o \
	src/main.o \
	src/reqparser.o \
	src/sys.o \
	src/util.o

CC := clang
CCFLAGS := -ffreestanding -nostdlib -fno-stack-protector -flto -static -O3

LD := ${CC}
LDFLAGS := ${CCFLAGS} -fuse-ld=lld 

.PHONY: all
all: http2sd

.PHONY: clean
clean:
	rm -f ${object_files} http2sd

%.o: %.c
	${CC} $^ -c -o $@ ${CCFLAGS}

http2sd: ${object_files}
	${LD} $^ -o $@ ${LDFLAGS}
	strip --strip-all $@
	objcopy --remove-section .comment $@
