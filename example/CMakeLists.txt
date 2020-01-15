cmake_minimum_required(VERSION 3.8)
project (echo)
 
# include library
include_directories(../include)

# verbose
set(CMAKE_VERBOSE_MAKEFILE off)

# include library
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build type ${CMAKE_BUILD_TYPE}")

# compiler specific flags
if (MSVC)
  add_compile_options(/W4 /WX)      # warning level 4 and all warnings as errors
  add_compile_options(/EHsc)        # no exceptions
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror) # warnings / errors
  add_compile_options(-fno-exceptions)                 # no exceptions
endif()

# c++ standard
set(CMAKE_CXX_STANDARD 17)
 
# sources
set(CLIENT_SRC echo-client.cpp)
set(SERVER_SRC echo-server.cpp)

# Executable
add_executable(client ${CLIENT_SRC})
add_executable(server ${SERVER_SRC})

