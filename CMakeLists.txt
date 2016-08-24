# minimum required version of CMake
cmake_minimum_required(VERSION 2.8.9)

# Project name
project(BeatIt)
set (BeatIt_VERSION_MAJOR 0)
set (BeatIt_VERSION_MINOR 0)




# 

# Packages 
find_package(MPI REQUIRED)
find_package(VTK REQUIRED)
find_package(Boost REQUIRED)
message("-- VTK_INCLUDE_DIRS: ${VTK_INCLUDE_DIRS}")
#message("VTK_LIBRARY_DIRS: ${VTK_LIBRARY_DIRS}")


#
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
message("-- Module path: ${CMAKE_MODULE_PATH}")
find_package(LIBMESH REQUIRED)

set(LIBMESH_LIB ${LIBMESH_LIBRARIES})




#include_directories(src)
include_directories ("${PROJECT_SOURCE_DIR}/src")
include_directories ("${LIBMESH_DIR}/include")
include_directories ("${VTK_INCLUDE_DIRS}")
include_directories ("${Boost_INCLUDE_DIR}")



# Include source directories
add_subdirectory(src)
add_subdirectory(testsuite)

# set flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")




# Summary 
message(" Compiling in ${CMAKE_BUILD_TYPE}")


get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()