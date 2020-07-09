cmake_minimum_required(VERSION 3.2)

project(OpenNfsC
        VERSION 1.0.0)

set (CMAKE_CXX_STANDARD 11)

set(SOVERSION 1 CACHE STRING "" FORCE)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_CLIENTS "Build client programs" ON)

include_directories(include
                    include/nfsrpc)

add_subdirectory(src)

if(BUILD_CLIENTS)
  add_subdirectory(ClientV4)
  add_subdirectory(ClientV3)
endif()
