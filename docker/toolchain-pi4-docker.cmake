# Cross-compilation toolchain for Raspberry Pi 4 (aarch64) — inside Docker container
# Used by cross-build.sh, not intended for direct use

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Use arm64 system paths for target libs
set(CMAKE_SYSROOT "")
set(CMAKE_FIND_ROOT_PATH "/usr/lib/aarch64-linux-gnu;/usr/aarch64-linux-gnu")

# Host Qt tools (moc, rcc, etc.) are native x86_64 in the container
set(QT_HOST_PATH "/usr" CACHE PATH "Host Qt6 tools")

# Search target paths for libs/headers, host for build tools
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# pkg-config for arm64 target
set(ENV{PKG_CONFIG_PATH} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
