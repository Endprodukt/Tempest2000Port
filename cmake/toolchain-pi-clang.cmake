# The Pi build without WSL: clang and lld on the PC, against a copy of the
# cabinet's own libraries and headers (build/pi-sysroot, see BUILD.md).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(T2K_PI_SYSROOT "${CMAKE_CURRENT_LIST_DIR}/../build/pi-sysroot" CACHE PATH "Copy of the cabinet's /usr and /lib")
get_filename_component(T2K_PI_SYSROOT "${T2K_PI_SYSROOT}" ABSOLUTE)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_SYSROOT "${T2K_PI_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH "${T2K_PI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# SDL2 comes from the sysroot as a ready target (see CMakeLists.txt).
set(CMAKE_DISABLE_FIND_PACKAGE_SDL2 TRUE)
set(T2K_PI_BUILD TRUE)
