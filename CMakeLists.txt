cmake_minimum_required(VERSION 3.10)
project(lvgl)

set(CMAKE_C_STANDARD 99) # C99 - lvgl officially supports C99 and above
set(CMAKE_CXX_STANDARD 17) # C17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNL REQUIRED IMPORTED_TARGET libnl-3.0 libnl-genl-3.0 libnl-route-3.0)
pkg_check_modules(CJSON REQUIRED IMPORTED_TARGET libcjson)
pkg_check_modules(BPF REQUIRED IMPORTED_TARGET libbpf)

add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${LIBNL_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} ${BPF_INCLUDE_DIRS})

file(GLOB VX_SOURCES vx_*.c vx_*.h)
add_executable(main main.c ${VX_SOURCES})

# Add paths to the static libraries if they are not in the standard locations
link_directories(/usr/local/lib /usr/lib/x86_64-linux-gnu)

# Set flags for static linking
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_FIND_LIBRARY_SUFFIXES .a) # Prefer static libraries
set(BUILD_SHARED_LIBS OFF) # Ensure static linkage

# Find and link static versions of the libraries
find_library(LIBNL_STATIC libnl-3.a)
find_library(LIBNL_GENL_STATIC libnl-genl-3.a)
find_library(LIBNL_ROUTE_STATIC libnl-route-3.a)
find_library(CJSON_STATIC libcjson.a)
find_library(BPF_STATIC libbpf.a)
find_library(LIBELF_STATIC libelf.a)
find_library(LIBZ_STATIC libz.a)
#find_library(LIBZSTD_STATIC libzstd.a)

# Link the static libraries including libelf.a
target_link_libraries(main lvgl m pthread
    ${LIBNL_STATIC} ${LIBNL_GENL_STATIC} ${LIBNL_ROUTE_STATIC}
    ${CJSON_STATIC} ${BPF_STATIC} ${LIBELF_STATIC} ${LIBZ_STATIC} ${LIBZSTD_STATIC}
)

add_custom_target(run COMMAND ${EXECUTABLE_OUTPUT_PATH}/main DEPENDS main)

target_compile_options(main PUBLIC ${LIBNL_CFLAGS_OTHER} ${CJSON_CFLAGS_OTHER} ${BPF_CFLAGS_OTHER})
