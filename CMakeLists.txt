project(bson_bind)

cmake_minimum_required(VERSION 2.8)

if(NOT WIN32)
  set(CXX_FLAGS "--std=c++11")
endif()

add_executable(bson_bind bson_bind.cpp)

install(FILES option.hpp DESTINATION include/bson_bind)
install(TARGETS bson_bind DESTINATION bin)
