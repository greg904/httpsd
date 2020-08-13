cmake_minimum_required(VERSION 3.16.3)
project(http2s C)

include(GNUInstallDirs)
include(cmake/FindSystemD.cmake)

option(WITH_SYSTEMD "install the systemd service file" ${SYSTEMD_FOUND})

add_subdirectory(src)

# systemd service file
if (WITH_SYSTEMD)
	configure_file(
		"${PROJECT_SOURCE_DIR}/data/http2s.service.in"
		"${PROJECT_BINARY_DIR}/data/http2s.service"
	)

	install(FILES "${PROJECT_BINARY_DIR}/data/http2s.service"
		DESTINATION ${INSTALL_SYSTEMD_UNITDIR})
endif()
