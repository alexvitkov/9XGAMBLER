cmake_minimum_required(VERSION 4.0)
project(9XGAMBLER)

add_executable(9XGAMBLER
    9xgambler.cpp)


set(raylib_USE_STATIC_LIBS ON CACHE BOOL "")
add_subdirectory(vendor/raylib-5.5)

target_link_libraries(9XGAMBLER raylib)
set_property(TARGET 9XGAMBLER PROPERTY CXX_STANDARD 20)
