cmake_minimum_required(VERSION 3.20)
project(cad_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tell CMake where OCCT is installed
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../../third_party/occt-install")

find_package(OpenCASCADE REQUIRED)

add_executable(cad_core
  src/app.cpp
  src/core/box_feature.cpp
  src/core/cylinder_feature.cpp
  src/core/document.cpp
  src/core/export.cpp
  src/core/extrude_feature.cpp
  src/core/sketch_feature.cpp
  src/core/sketch_profile.cpp
  src/core/viewport.cpp
  src/protocol/ipc.cpp
  src/protocol/serialization.cpp
  src/main.cpp
)

target_include_directories(cad_core PRIVATE
  include
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
)

target_link_libraries(cad_core PRIVATE ${OpenCASCADE_LIBRARIES})

add_executable(cad_core_sketch_profile_test
  src/core/sketch_feature.cpp
  src/core/sketch_profile.cpp
  tests/sketch_profile_test.cpp
)

target_include_directories(cad_core_sketch_profile_test PRIVATE
  include
  src
)
