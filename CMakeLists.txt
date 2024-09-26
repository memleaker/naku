cmake_minimum_required(VERSION 3.30)

# test 需要放在头部
# add_subdirectory(${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/tests)

# project
project(naku)

# cpp standard
set(CMAKE_CXX_STANDARD 20)

# headers
include_directories(include)

# source
file(GLOB_RECURSE SOURCES "naku/*")

# compiler
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

# compiler flag
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-g -Wall -fcoroutines")

# traget
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_SOURCE_DIR})
add_library(naku SHARED ${SOURCES})
set_target_properties(naku PROPERTIES LINKER_LANGUAGE CXX)

# ld
#target_link_libraries(libnaku.so pthread)
#target_link_libraries(naku pthread ssl crypto)

# install
install(FILES include/* DESTINATION /usr/include)
install(FILES libnaku.so DESTINATION /usr/lib)
