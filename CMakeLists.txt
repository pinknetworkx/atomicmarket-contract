# This file is used by compile.sh with a modern CDT. At the moment we're not sure we can trust the
# build, so compile_old_cdt.sh should be used.

cmake_minimum_required(VERSION 3.16)
project(atomicmarket VERSION 1.0.0)

include(CTest)
find_package(cdt)

# generate list of all C++ source files in /src folder
file(GLOB SRC_CPP_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# The first parameter is the contract name, the second is the cmake target name,
# and the rest are the CPP files needed to build the contract.
add_contract( atomicmarket atomicmarket ${SRC_CPP_FILES} )

target_include_directories( atomicmarket PUBLIC ${CMAKE_SOURCE_DIR}/include )
target_ricardian_directory( atomicmarket ${CMAKE_SOURCE_DIR}/resource )
