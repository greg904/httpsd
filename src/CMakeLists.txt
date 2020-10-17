add_executable(http2s
	cli.c
	cli.h
	conn.c
	conn.h
	epoll.c
	epoll.h
	main.c
	reqparser.c
	reqparser.h
	sys.c
	sys.h
	util.c
	util.h)

target_compile_options(http2s PRIVATE -ffreestanding -nostdlib)
target_link_libraries(http2s PRIVATE -ffreestanding -nostdlib)

install(TARGETS http2s
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
