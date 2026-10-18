# Configure with [fxsdk build-fx] or [fxsdk build-cg]
cmake_minimum_required(VERSION 3.15)
project(Base-N)

include(GenerateG1A)
include(GenerateG3A)
include(Fxconv)
find_package(Gint 2.9 REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../common/common.cmake)

casio_app(Base-N
  SOURCES
    src/main.c
  ASSETS_fx
    assets-fx/example.png
  ASSETS_cg
    assets-cg/example.png
)
