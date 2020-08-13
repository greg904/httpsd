add_executable(http2s
	conn.c
	conn.h
	epoll.c
	epoll.h
	main.c
	reqparser.c
	reqparser.h
	util.c
	util.h)

install(TARGETS http2s
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
