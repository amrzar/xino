# ukernel/host_tools/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(host_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sysreg_gen src/sysreg_gen.cpp)
