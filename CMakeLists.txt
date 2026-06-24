cmake_minimum_required(VERSION 3.10)
project(deb2pkg LANGUAGES CXX)

# 编译器: LLVM/Clang + musl libc
find_program(CLANG_CXX clang++)
if(NOT CLANG_CXX)
    message(FATAL_ERROR "未找到 clang++，请安装: sudo pacman -S clang")
endif()
set(CMAKE_CXX_COMPILER "${CLANG_CXX}")
message(STATUS "使用编译器: ${CLANG_CXX}")

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
# --target=x86_64-linux-musl: 使用 musl libc
# -O2: 优化级别
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-linux-musl -O2 -Wall -Wextra -pedantic")
# musl 链接路径
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/lib/musl/lib")

# 查找 libarchive
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
if(NOT LIBARCHIVE_FOUND)
    message(FATAL_ERROR "需要 libarchive 开发库。请运行: sudo pacman -S libarchive")
endif()

# 源文件列表
set(SOURCES
    src/main.cpp
    src/core/logger.cpp
    src/core/executor.cpp
    src/extract/archive_reader.cpp
    src/extract/deb_extractor.cpp
    src/extract/rpm_extractor.cpp
    src/metadata/deb_control_parser.cpp
    src/metadata/rpm_header_parser.cpp
    src/transform/path_fixer.cpp
    src/transform/elf_dependency_scanner.cpp
    src/pkgfile/pkgfile_resolver.cpp
    src/generate/pkgbuild_writer.cpp
    src/generate/makepkg_builder.cpp
)

add_executable(deb2pkg ${SOURCES})

target_include_directories(deb2pkg PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(deb2pkg PRIVATE
    ${LIBARCHIVE_LIBRARIES}
)

target_link_options(deb2pkg PRIVATE
    ${LIBARCHIVE_LDFLAGS}
)

install(TARGETS deb2pkg DESTINATION /usr/bin)
