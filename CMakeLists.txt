cmake_minimum_required(VERSION 3.13)
project(g9-https-redirect C)

add_executable(g9-https-redirect src/conn.c src/conn.h src/epoll.c src/epoll.h
	src/main.c src/reqparser.c src/reqparser.h src/util.c src/util.h)

install(TARGETS g9-https-redirect
	RUNTIME DESTINATION bin)
