find_package(PkgConfig REQUIRED)

pkg_check_modules(SYSTEMD systemd)

# Find systemd unit directory (for service files)
if (SYSTEMD_FOUND AND NOT INSTALL_SYSTEMD_UNITDIR)
        pkg_get_variable(INSTALL_SYSTEMD_UNITDIR systemd systemdsystemunitdir)
endif()
