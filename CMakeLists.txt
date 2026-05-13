cmake_minimum_required(VERSION 3.14)
project(scanner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(scanner main.cpp scan.h)

target_include_directories(scanner PRIVATE ${CMAKE_SOURCE_DIR}/lib)