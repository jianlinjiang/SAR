# aux_source_directory(. DIR_LIB_SRCS)
cmake_minimum_required(VERSION 2.8.11)
set(DIR_LIB_SRCS main.cpp)
set(LOCAL_INCLUDE /usr/local/include)

include_directories(${LOCAL_INCLUDE} ${CMAKE_INCLUDE_CURRENT_DIR})

add_executable(network ${DIR_LIB_SRCS})