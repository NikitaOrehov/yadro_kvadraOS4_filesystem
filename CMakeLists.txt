cmake_minimum_required(VERSION 3.14)
project(media_scanner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(media_scanner main.cpp scan.h)

target_include_directories(media_scanner PRIVATE ${CMAKE_SOURCE_DIR}/lib)