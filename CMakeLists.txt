cmake_minimum_required(VERSION 3.21)

project(tempest2000 VERSION 0.3.0 LANGUAGES C CXX)

# Tempest 2000 for VectorPie. Built like the SDK's own programs:
#   PC, natively (MSYS2 MinGW-w64 with SDL2):  see BUILD.md
#   PC and Pi from WSL2 Debian trixie:         the SDK's toolchain files
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(T2K_PI_BUILD)
    # cmake/toolchain-pi-clang.cmake: the cabinet's own SDL2, by its file.
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
        IMPORTED_SONAME "libSDL2-2.0.so.0"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include/SDL2"
        INTERFACE_COMPILE_DEFINITIONS "_REENTRANT")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/VectorPie/sdk vectorpie)

# The release edition holds none of Atari's data: it takes it from the
# player's Jaguar ROM (tools/make_rom_data.py, src/rom.c). Build it in a
# folder of its own: -DT2K_ROM_DATA=ON.
option(T2K_ROM_DATA "Release edition: the game's data from the player's ROM" OFF)
if(T2K_ROM_DATA)
    set(T2K_DATA_SOURCES src/gen/t2k_data_rom.c src/rom.c)
else()
    set(T2K_DATA_SOURCES src/gen/t2k_data.c)
endif()

add_executable(tempest2000
    src/main.c
    src/game.c
    src/logo.c
    src/mod.c
    src/service.c
    src/vpbind.c
    src/sound.c
    src/view.c
    src/words.c
    ${T2K_DATA_SOURCES})
target_include_directories(tempest2000 PRIVATE src)
if(T2K_ROM_DATA)
    target_compile_definitions(tempest2000 PRIVATE T2K_ROM_DATA)
endif()
vp_program(tempest2000)

# The package's manifest, which also holds the spinner's sensitivity.
add_custom_command(TARGET tempest2000 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/package/game.ini
            $<TARGET_FILE_DIR:tempest2000>/game.ini)

# The sound, made once by tools/extract_audio.py: next to the executable,
# where vp_asset_path finds it.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets AND NOT T2K_ROM_DATA)
    add_custom_command(TARGET tempest2000 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:tempest2000>/assets)
endif()

