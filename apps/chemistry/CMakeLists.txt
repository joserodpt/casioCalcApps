# Configure with [fxsdk build-fx] or [fxsdk build-cg]
cmake_minimum_required(VERSION 3.15)
project(Chemistry)

include(GenerateG1A)
include(GenerateG3A)
include(Fxconv)
find_package(Gint 2.9 REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

casio_app(Chemistry NAME_PT "Quimica"
  SOURCES
    src/main.c
    src/periodic.c
    src/periodic_data.c
)
