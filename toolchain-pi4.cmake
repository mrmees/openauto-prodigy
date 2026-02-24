# Cross-compilation toolchain for Raspberry Pi 4 (aarch64)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-pi4.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Pi sysroot (rsync'd from target)
set(CMAKE_SYSROOT $ENV{HOME}/pi-sysroot)

# Search the sysroot for libraries/headers, but use host tools (like cmake, pkg-config)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Qt6 cross-compilation: use local host tools (moc, rcc, syncqt) but target sysroot for libs/headers
set(QT_HOST_PATH "/usr" CACHE PATH "Path to host Qt6 installation (for moc, rcc, etc.)")

# Help pkg-config find .pc files in the sysroot
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
