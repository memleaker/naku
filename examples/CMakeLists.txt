cmake_minimum_required(VERSION 3.30)

project(tests)

# cpp standard
set(CMAKE_CXX_STANDARD 20)

# headers
include_directories(../include)

# compiler
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

# compiler flag
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-g -Wall -fcoroutines")

# target
add_executable(echoserver echoserver.cpp)
target_link_libraries(echoserver pthread naku)
