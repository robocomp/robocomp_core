cmake_minimum_required(VERSION 3.26)
project(doublebuffer_sync)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

add_executable(doublebuffer_sync
        doublebuffer_sync.h
        main.cpp)
