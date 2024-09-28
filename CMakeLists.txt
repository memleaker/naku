cmake_minimum_required(VERSION 3.30)

# project
project(naku)

# cpp standard
set(CMAKE_CXX_STANDARD 20)

# headers
include_directories(include)

# source
file(GLOB_RECURSE SOURCES "src/*")

# compiler
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

# compiler flag
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-g -Wall -fcoroutines")

# traget
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_SOURCE_DIR})
add_library(naku SHARED ${SOURCES})
set_target_properties(naku PROPERTIES LINKER_LANGUAGE CXX)

# install
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/naku/ DESTINATION /usr/include/)
install(FILES ${CMAKE_SOURCE_DIR}/libnaku.so DESTINATION /usr/lib)
