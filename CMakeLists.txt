cmake_minimum_required(VERSION 3.12)

project(simple_perftest)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(Threads REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)

include(FetchContent)
FetchContent_Declare(current URL https://github.com/c5t/current/archive/ced2506fa18094cb15281671726420a4947fdcb9.zip)
FetchContent_MakeAvailable(current)
include_directories(${current_SOURCE_DIR})

file(GLOB SOURCE_FILES "*.cc" "*/*.cc")
file(GLOB HEADER_FILES "*.h")
foreach(SOURCE_FILE ${SOURCE_FILES})
  get_filename_component(BINARY_FILE "${SOURCE_FILE}" NAME_WE)
  add_executable(${BINARY_FILE} ${SOURCE_FILE} ${HEADER_FILES})
  target_link_libraries(${BINARY_FILE} PRIVATE Threads::Threads)
endforeach()
