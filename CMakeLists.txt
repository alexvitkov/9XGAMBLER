cmake_minimum_required(VERSION 4.0)
project(slots)

add_executable(slots
    9xgambler.cpp)


set(raylib_USE_STATIC_LIBS ON CACHE BOOL "")
add_subdirectory(vendor/raylib-5.5)

target_link_libraries(slots raylib)
set_property(TARGET slots PROPERTY CXX_STANDARD 20)
