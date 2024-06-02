cmake_minimum_required(VERSION 3.26)
project(new_doublebuffer)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

add_executable(new_doublebuffer
        doublebuffer.h
        main.cpp)
