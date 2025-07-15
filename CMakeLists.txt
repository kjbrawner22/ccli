cmake_minimum_required(VERSION 3.26)
project(ccli VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ccli
  src/ccli.c
)

target_include_directories(ccli PRIVATE
  include
)