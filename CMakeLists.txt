cmake_minimum_required(VERSION 3.13)
project(http2s C)

add_executable(http2s
	src/conn.c
	src/conn.h
	src/epoll.c
	src/epoll.h
	src/main.c
	src/reqparser.c
	src/reqparser.h
	src/util.c
	src/util.h)

install(TARGETS http2s
	RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES http2s.service
	DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
