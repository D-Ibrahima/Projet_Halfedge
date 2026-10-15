cmake_minimum_required(VERSION 3.10)
project(halfedge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Eigen3 REQUIRED NO_MODULE)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(assimp REQUIRED)

add_executable(halfedge structure.cpp)
target_compile_options(halfedge PRIVATE -Wall -Wextra)
target_link_libraries(halfedge PRIVATE
  Eigen3::Eigen
  OpenGL::GL
  glfw
  assimp::assimp)