cmake_minimum_required(VERSION 3.21)
project(Injection)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-DUNICODE)

add_library(testDll SHARED testDll.cpp)
add_executable(Injection WIN32 injection.cpp PETools/PETools.h PETools/PETools.cpp)
