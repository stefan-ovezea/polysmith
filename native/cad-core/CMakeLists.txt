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
  ${EIGEN3_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)
target_compile_definitions(planegcs PRIVATE BOOST_ALLOW_DEPRECATED_HEADERS)
# planegcs uses 'register' keyword (removed in C++17) in some Eigen internals
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(planegcs PRIVATE -Wno-register)
endif()

add_executable(cad_core
  src/app.cpp
  src/core/body_compiler.cpp
  src/core/box_feature.cpp
  src/core/cam_operation.cpp
  src/core/face_milling.cpp
  src/core/construction_plane_feature.cpp
  src/core/constraint_solver.cpp
  src/core/cylinder_feature.cpp
  src/core/dof_counter.cpp
  src/core/document.cpp
  src/core/edge_geometry.cpp
  src/core/export.cpp
  src/core/extrude_feature.cpp
  src/core/face_geometry.cpp
  src/core/feature_shape.cpp
  src/core/formula_eval.cpp
  src/core/inference_engine.cpp
  src/core/loft_feature.cpp
  src/core/logger.cpp
  src/core/refresh_dependents.cpp
  src/core/revolve_feature.cpp
  src/core/sketch_feature.cpp
  src/core/sketch_profile.cpp
  src/core/sweep_feature.cpp
  src/core/trim_engine.cpp
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

target_link_libraries(cad_core PRIVATE ${OpenCASCADE_LIBRARIES} planegcs)

add_executable(cad_core_sketch_profile_test
  src/core/constraint_solver.cpp
  src/core/formula_eval.cpp
  src/core/inference_engine.cpp
  src/core/sketch_feature.cpp
  src/core/sketch_profile.cpp
  src/core/trim_engine.cpp
  tests/sketch_profile_test.cpp
)

target_include_directories(cad_core_sketch_profile_test PRIVATE
  include
  src
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${EIGEN3_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_sketch_profile_test PRIVATE
  planegcs
)

add_executable(cad_core_multi_profile_extrude_test
  src/core/body_compiler.cpp
  src/core/box_feature.cpp
  src/core/cam_operation.cpp
  src/core/face_milling.cpp
  src/core/construction_plane_feature.cpp
  src/core/constraint_solver.cpp
  src/core/cylinder_feature.cpp
  src/core/dof_counter.cpp
  src/core/document.cpp
  src/core/edge_geometry.cpp
  src/core/export.cpp
  src/core/extrude_feature.cpp
  src/core/face_geometry.cpp
  src/core/feature_shape.cpp
  src/core/formula_eval.cpp
  src/core/inference_engine.cpp
  src/core/loft_feature.cpp
  src/core/logger.cpp
  src/core/refresh_dependents.cpp
  src/core/revolve_feature.cpp
  src/core/sketch_feature.cpp
  src/core/sketch_profile.cpp
  src/core/sweep_feature.cpp
  src/core/trim_engine.cpp
  src/core/viewport.cpp
  src/protocol/ipc.cpp
  src/protocol/serialization.cpp
  tests/multi_profile_extrude_test.cpp
)

target_include_directories(cad_core_multi_profile_extrude_test PRIVATE
  include
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${EIGEN3_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_multi_profile_extrude_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
)

add_executable(cad_core_cam_face_reference_test
  src/core/body_compiler.cpp
  src/core/box_feature.cpp
  src/core/cam_operation.cpp
  src/core/face_milling.cpp
  src/core/construction_plane_feature.cpp
  src/core/cylinder_feature.cpp
  src/core/dof_counter.cpp
  src/core/document.cpp
  src/core/edge_geometry.cpp
  src/core/export.cpp
  src/core/extrude_feature.cpp
  src/core/face_geometry.cpp
  src/core/feature_shape.cpp
  src/core/formula_eval.cpp
  src/core/inference_engine.cpp
  src/core/loft_feature.cpp
  src/core/logger.cpp
  src/core/refresh_dependents.cpp
  src/core/revolve_feature.cpp
  src/core/sketch_feature.cpp
  src/core/sketch_profile.cpp
  src/core/snap_engine.cpp
  src/core/sweep_feature.cpp
  src/core/trim_engine.cpp
  src/core/viewport.cpp
  src/protocol/ipc.cpp
  src/protocol/serialization.cpp
  tests/cam_face_reference_test.cpp
)

target_include_directories(cad_core_cam_face_reference_test PRIVATE
  include
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
)

target_link_libraries(cad_core_cam_face_reference_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
)
