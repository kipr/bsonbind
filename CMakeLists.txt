project(bson_bind)

cmake_minimum_required(VERSION 3.0)

add_definitions(-std=c++11)

add_executable(bson_bind bson_bind.cpp)

install(FILES option.hpp DESTINATION include/bson_bind)
install(PROGRAMS ${CMAKE_BINARY_DIR}/bson_bind DESTINATION bin)