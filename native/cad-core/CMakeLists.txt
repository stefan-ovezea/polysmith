cmake_minimum_required(VERSION 3.20)
project(cad_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC needs _USE_MATH_DEFINES for M_PI
if(MSVC)
  add_compile_definitions(_USE_MATH_DEFINES)
endif()

# Place binaries directly in build/ (not build/Debug/) so Tauri finds them
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Tell CMake where OCCT is installed
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../../third_party/occt-install")

find_package(OpenCASCADE REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED)

if(UNIX AND NOT APPLE)
  # Use RPATH instead of RUNPATH so transitive dependencies are found
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
endif()

# --- planegcs static library (2D geometric constraint solver from FreeCAD) ---
set(PLANEGCS_DIR "${CMAKE_SOURCE_DIR}/../../third_party/planegcs/planegcs")
add_library(planegcs STATIC
  ${PLANEGCS_DIR}/GCS.cpp
  ${PLANEGCS_DIR}/Geo.cpp
  ${PLANEGCS_DIR}/Constraints.cpp
  ${PLANEGCS_DIR}/SubSystem.cpp
  ${PLANEGCS_DIR}/qp_eq.cpp
)
target_include_directories(planegcs PUBLIC
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)
target_link_libraries(planegcs PUBLIC Eigen3::Eigen)
target_compile_definitions(planegcs PRIVATE BOOST_ALLOW_DEPRECATED_HEADERS)
# planegcs uses 'register' keyword (removed in C++17) in some Eigen internals
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(planegcs PRIVATE -Wno-register)
endif()

set(CAD_CORE_SOURCES
  src/core/cam/cam_operation.cpp
  src/core/cam/face_milling.cpp
  src/core/construction/construction_plane_feature.cpp
  src/core/diagnostics/logger.cpp
  src/core/document/document.cpp
  src/core/export/export.cpp
  src/core/extrude/extrude_feature.cpp
  src/core/geometry/body_compiler.cpp
  src/core/geometry/edge_geometry.cpp
  src/core/geometry/face_geometry.cpp
  src/core/geometry/feature_shape.cpp
  src/core/geometry/refresh_dependents.cpp
  src/core/loft/loft_feature.cpp
  src/core/plugin/plugin_feature.cpp
  src/core/primitive/box_feature.cpp
  src/core/primitive/cylinder_feature.cpp
  src/core/revolve/revolve_feature.cpp
  src/core/sketch/constraint_solver.cpp
  src/core/sketch/dof_counter.cpp
  src/core/sketch/formula_eval.cpp
  src/core/sketch/inference_engine.cpp
  src/core/sketch/sketch_feature.cpp
  src/core/sketch/sketch_profile.cpp
  src/core/sketch/trim_engine.cpp
  src/core/sweep/sweep_feature.cpp
  src/core/viewport/viewport.cpp
  src/protocol/ipc.cpp
  src/protocol/serialization.cpp
)

add_executable(cad_core
  src/app.cpp
  ${CAD_CORE_SOURCES}
  src/main.cpp
)

target_include_directories(cad_core PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
)

target_link_libraries(cad_core PRIVATE ${OpenCASCADE_LIBRARIES} planegcs)

add_executable(cad_core_sketch_profile_test
  src/core/sketch/constraint_solver.cpp
  src/core/sketch/formula_eval.cpp
  src/core/sketch/inference_engine.cpp
  src/core/sketch/sketch_feature.cpp
  src/core/sketch/sketch_profile.cpp
  src/core/sketch/trim_engine.cpp
  tests/sketch_profile_test.cpp
)

target_include_directories(cad_core_sketch_profile_test PRIVATE
  src
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_sketch_profile_test PRIVATE
  planegcs
)

add_executable(cad_core_multi_profile_extrude_test
  ${CAD_CORE_SOURCES}
  tests/multi_profile_extrude_test.cpp
)

target_include_directories(cad_core_multi_profile_extrude_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_multi_profile_extrude_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
)

add_executable(cad_core_cam_face_reference_test
  ${CAD_CORE_SOURCES}
  tests/cam_face_reference_test.cpp
)

target_include_directories(cad_core_cam_face_reference_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_cam_face_reference_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
)

add_executable(cad_core_plugin_feature_test
  ${CAD_CORE_SOURCES}
  tests/plugin_feature_test.cpp
)

target_include_directories(cad_core_plugin_feature_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_plugin_feature_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
)
