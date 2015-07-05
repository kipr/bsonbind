project(bson_bind)

cmake_minimum_required(VERSION 2.8)

add_definitions(-std=c++11)

add_executable(bson_bind bson_bind.cpp)

add_definitions(-g)

install(FILES option.hpp DESTINATION include/bson_bind)
install(PROGRAMS ${CMAKE_BINARY_DIR}/bson_bind DESTINATION bin)