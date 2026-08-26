cmake_minimum_required(VERSION 3.20)
project(cad_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC needs _USE_MATH_DEFINES for M_PI and /EHsc for exception
# handling — without it, Boost defines BOOST_NO_EXCEPTIONS which makes
# boost::throw_exception an unresolved external.
if(MSVC)
  add_compile_definitions(_USE_MATH_DEFINES)
  add_compile_options(/EHsc)
endif()

# Place binaries directly in build/ (not build/Release/) so Tauri finds them.
# Must be CACHE variables for Visual Studio multi-config generator.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR} CACHE PATH "" FORCE)
endforeach()

# ---- OpenCASCADE ---------------------------------------------------------
# Self-built OCCT 8 at third_party/occt8-build.  Use a manual INTERFACE
# target — avoids cmake install issues (per-module target files are only
# generated during install, which partially failed due to locked DLLs).
set(OCCT_ROOT "${CMAKE_SOURCE_DIR}/../../third_party/occt8-build")
# OCCT's in-tree build dir (occt8-build/inc) carries most headers, but
# the TColStd/TColgp array templates are deprecated headers that only
# land in the install tree — the spline helpers (spline_profile_occt.cpp,
# the BSpline wire edges) need them. Headers are platform-independent.
include_directories("${CMAKE_SOURCE_DIR}/../../third_party/occt8-install/inc")
add_library(occt_iface INTERFACE)
# inc/ is the build-tree header location — works on both platforms without
# requiring cmake --install (which fails on Windows due to locked DLLs).
target_include_directories(occt_iface INTERFACE "${OCCT_ROOT}/inc")
if(WIN32)
  target_link_directories(occt_iface INTERFACE "${OCCT_ROOT}/win64/vc14/lib")
elseif(APPLE)
  target_link_directories(occt_iface INTERFACE "${OCCT_ROOT}/mac64/clang/lib")
else()
  target_link_directories(occt_iface INTERFACE "${OCCT_ROOT}/lin64/gcc/lib")
endif()
target_link_libraries(occt_iface INTERFACE
  # FoundationClasses
  TKernel TKMath
  # ModelingData
  TKG2d TKG3d TKGeomBase TKBRep
  # ModelingAlgorithms
  TKGeomAlgo TKTopAlgo TKPrim TKBO TKShHealing TKBool
  TKHLR TKHelix TKFillet TKOffset TKFeat TKMesh TKExpress TKXMesh
  # ApplicationFramework
  TKCDF TKLCAF TKCAF TKBinL TKXmlL TKBin TKXml
  TKStdL TKStd TKTObj TKBinTObj TKXmlTObj TKVCAF
  # DataExchange
  TKDE TKXSBase TKDESTEP TKXCAF TKDEIGES TKDESTL TKDEVRML
  TKRWMesh TKDECascade TKBinXCAF TKXmlXCAF TKDEOBJ TKDEPLY
  # Visualization — TKService provides the font stack (Font_BRepFont /
  # Font_FTFont / Font_FontMgr, FreeType linked statically), TKV3d owns
  # StdPrs_BRepFont. Needed by the sketch text engine.
  TKService TKV3d
)
set(OpenCASCADE_LIBRARIES occt_iface)
find_package(Eigen3 REQUIRED)
# CMake 3.30+ removed FindBoost.cmake (CMP0167).  vcpkg provides
# BoostConfig.cmake, so use CONFIG mode explicitly.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
  cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost CONFIG REQUIRED)

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
target_link_libraries(planegcs PUBLIC Eigen3::Eigen Boost::headers)
target_compile_definitions(planegcs PRIVATE BOOST_ALLOW_DEPRECATED_HEADERS)
# planegcs uses 'register' keyword (removed in C++17) in some Eigen internals
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(planegcs PRIVATE -Wno-register)
endif()

# --- libdxfrw static library (DXF read/write, DXF-only subset) ---
set(LIBDXFRW_DIR "${CMAKE_SOURCE_DIR}/../../third_party/libdxfrw")
add_library(dxfrw STATIC
  ${LIBDXFRW_DIR}/src/drw_classes.cpp
  ${LIBDXFRW_DIR}/src/drw_entities.cpp
  ${LIBDXFRW_DIR}/src/drw_header.cpp
  ${LIBDXFRW_DIR}/src/drw_objects.cpp
  ${LIBDXFRW_DIR}/src/intern/drw_dbg.cpp
  # dwgBuffer/dwgutil are plain byte-buffer/hex-string helpers the
  # entity classes reference from their (unused in DXF builds)
  # parseDwg paths — needed at link time once any DRW_Entity subclass
  # is actually consumed. No iconv or DWG-reader dependency.
  ${LIBDXFRW_DIR}/src/intern/dwgBuffer.cpp
  ${LIBDXFRW_DIR}/src/intern/dwgutil.cpp
  ${LIBDXFRW_DIR}/src/intern/dxfreader.cpp
  ${LIBDXFRW_DIR}/src/intern/dxfwriter.cpp
  ${LIBDXFRW_DIR}/src/intern/rscodec.cpp
  ${LIBDXFRW_DIR}/src/libdxfrw.cpp
  # Stub replaces iconv-dependent drw_textcodec.cpp
  src/dxf/drw_textcodec_stub.cpp
)
target_include_directories(dxfrw PUBLIC ${LIBDXFRW_DIR}/src)

set(CAD_CORE_SOURCES
  src/core/cam/cam_operation.cpp
  src/core/cam/face_milling.cpp
  src/core/construction/construction_plane_feature.cpp
  src/core/diagnostics/logger.cpp
  src/core/document/document.cpp
  src/core/export/export.cpp
  src/dxf/dxf_export.cpp
  src/dxf/dxf_import.cpp
  src/core/extrude/extrude_feature.cpp
  src/core/geometry/body_compiler.cpp
  src/core/geometry/edge_geometry.cpp
  src/core/geometry/face_geometry.cpp
  src/core/geometry/feature_shape.cpp
  src/core/geometry/import_sew_helpers.cpp
  src/core/geometry/mesh_import_helpers.cpp
  src/core/geometry/mesh_projection.cpp
  src/core/geometry/refresh_dependents.cpp
  src/core/geometry/step_import_helpers.cpp
  src/core/geometry/iges_import_helpers.cpp
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
  src/core/sketch/spline_profile_occt.cpp
  src/core/sketch/trim_engine.cpp
  src/core/sweep/sweep_feature.cpp
  src/core/text_engine.cpp
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

target_link_libraries(cad_core PRIVATE ${OpenCASCADE_LIBRARIES} planegcs dxfrw)

# Tauri looks for cad_core.exe in build/ (not build/Release/).
add_custom_command(TARGET cad_core POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:cad_core>
    ${CMAKE_BINARY_DIR}/$<TARGET_FILE_NAME:cad_core>
  COMMENT "Copying cad_core to build/ for Tauri"
)

add_executable(cad_core_sketch_profile_test
  # Sketch code logs through the structured logger (e.g. the
  # PS_TRACE_FACES face-walk trace); this target is built without the
  # protocol dependency chain, so tests/logger_stub.cpp supplies
  # the logger symbols as a plain stderr writer.  Spline profile
  # intersections DO need OCCT (spline_profile_occt.cpp), so this
  # target links the OCCT libraries like the full-core suites.
  tests/logger_stub.cpp
  tests/text_engine_stub.cpp
  src/core/sketch/constraint_solver.cpp
  src/core/sketch/dof_counter.cpp
  src/core/sketch/formula_eval.cpp
  src/core/sketch/inference_engine.cpp
  src/core/sketch/sketch_feature.cpp
  src/core/sketch/sketch_profile.cpp
  src/core/sketch/spline_profile_occt.cpp
  src/core/sketch/trim_engine.cpp
  tests/sketch_profile_test.cpp
)

target_include_directories(cad_core_sketch_profile_test PRIVATE
  src
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
  # feature.h → body_feature_types.h now includes TopoDS_Shape.hxx
  # (step_import parameters); header-only, so no link change.
  ${OCCT_ROOT}/inc
)

target_link_libraries(cad_core_sketch_profile_test PRIVATE
  planegcs
  ${OpenCASCADE_LIBRARIES}
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
  dxfrw
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
  dxfrw
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
  dxfrw
)

add_executable(cad_core_extrude_quality_test
  ${CAD_CORE_SOURCES}
  tests/extrude_quality_test.cpp
)

target_include_directories(cad_core_extrude_quality_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_extrude_quality_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

add_executable(cad_core_sketch_move_test
  ${CAD_CORE_SOURCES}
  tests/sketch_move_test.cpp
)

target_include_directories(cad_core_sketch_move_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_sketch_move_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Ellipse entity regression suite — creation, exact profile edge,
# extrude smoke, move preservation, construction exclusion.
add_executable(cad_core_ellipse_test
  ${CAD_CORE_SOURCES}
  tests/ellipse_test.cpp
)

target_include_directories(cad_core_ellipse_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_ellipse_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Spline entity regression suite — control-point creation, pole drag
# re-fit, exact profile set, extrude smoke, move, trim rejection,
# save/load round-trip.
add_executable(cad_core_spline_test
  ${CAD_CORE_SOURCES}
  tests/spline_test.cpp
)

target_include_directories(cad_core_spline_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_spline_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Slot entity regression suite — expansion to 2 lines + 2 arcs,
# exact profile set, param update re-expansion, generated-entity
# guards, move, extrude smoke.
add_executable(cad_core_slot_test
  ${CAD_CORE_SOURCES}
  tests/slot_test.cpp
)

target_include_directories(cad_core_slot_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_slot_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Sketch chamfer regression suite — symmetric/asymmetric chamfers,
# distance edits (both epsilon sides), delete restores the corner,
# full profile set on a chamfered rectangle, fillet conflict.
add_executable(cad_core_sketch_chamfer_test
  ${CAD_CORE_SOURCES}
  tests/sketch_chamfer_test.cpp
)

target_include_directories(cad_core_sketch_chamfer_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_sketch_chamfer_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Sketch extend regression suite — line/arc extension both directions,
# closed-loop profile after extend, generated-entity rejection,
# constraint preservation, arc angle dim flips to driven.
add_executable(cad_core_extend_test
  ${CAD_CORE_SOURCES}
  tests/extend_test.cpp
)

target_include_directories(cad_core_extend_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_extend_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Sketch offset regression suite — line/circle/arc offsets on both
# distance signs, sweep preservation, collapse/invert rejection,
# construction/generated/unsupported rejection.
add_executable(cad_core_offset_test
  ${CAD_CORE_SOURCES}
  tests/offset_test.cpp
)

target_include_directories(cad_core_offset_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_offset_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Sketch transform regression suite — in-place scale (H/V preserved,
# radii scaled, dims flipped/re-measured), exploded copies (fresh
# ids, vertex topology preserved among copies, H/V stripped on
# rotated copies), single-undo, two-profile copy.
add_executable(cad_core_transform_test
  ${CAD_CORE_SOURCES}
  tests/transform_test.cpp
)

target_include_directories(cad_core_transform_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_transform_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Circle creation-mode regression suite — two/three-point circles,
# tangent-to-lines with circle-slave radius re-derivation, incenter,
# both wedge sides, full profile set.
add_executable(cad_core_circle_modes_test
  ${CAD_CORE_SOURCES}
  tests/circle_modes_test.cpp
)

target_include_directories(cad_core_circle_modes_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_circle_modes_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Dimension-completion regression suite — diameter display, arc
# angle sweeps, arc length driving.
add_executable(cad_core_dimension_completion_test
  ${CAD_CORE_SOURCES}
  tests/dimension_completion_test.cpp
)

target_include_directories(cad_core_dimension_completion_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_dimension_completion_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Sketch array regression suite — linear/circular exploded copies,
# unique ids, on-circle placement (1e-6), single-undo removal.
add_executable(cad_core_array_test
  ${CAD_CORE_SOURCES}
  tests/array_test.cpp
)

# Intersecting-arcs extrude regression — the lobe between two arcs
# must cut its bottom-cap hole, not wire it as a giant complement
# region (the "thin wall" bug, res/part.json geometry).
add_executable(cad_core_intersecting_arcs_extrude_test
  ${CAD_CORE_SOURCES}
  tests/intersecting_arcs_extrude_test.cpp
)

target_include_directories(cad_core_intersecting_arcs_extrude_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_intersecting_arcs_extrude_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

target_include_directories(cad_core_array_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_array_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Constraint-completion regression suite — symmetric, collinear,
# midpoint, tangent pairs, and parametric anchor-t enforcement.
add_executable(cad_core_constraint_completion_test
  ${CAD_CORE_SOURCES}
  tests/constraint_completion_test.cpp
)

target_include_directories(cad_core_constraint_completion_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_constraint_completion_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Parametric arc regression suite — GCS::Arc solver integration: driving
# radius/angle dimensions, shared-endpoint drag, over-constraint fallback,
# complete profile sets for mixed line+arc loops.
add_executable(cad_core_parametric_arc_test
  ${CAD_CORE_SOURCES}
  tests/parametric_arc_test.cpp
)

target_include_directories(cad_core_parametric_arc_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_parametric_arc_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Sketch tool whitelist regression suite — the two sketch-tool whitelists
# (validate_tool / is_supported_sketch_tool) must accept identical ids,
# and commands.schema.json must match the real command vocabulary.
add_executable(cad_core_tool_whitelist_test
  ${CAD_CORE_SOURCES}
  tests/tool_whitelist_test.cpp
)

target_include_directories(cad_core_tool_whitelist_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_tool_whitelist_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Selection/highlight regression suite — stale selection clears and the
# central prune pass (feature/highlight).
add_executable(cad_core_selection_test
  ${CAD_CORE_SOURCES}
  tests/cad_core_selection_test.cpp
)

target_include_directories(cad_core_selection_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_selection_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Face-projection arc recovery regression suite (feature/projection-arcs).
add_executable(cad_core_face_projection_arc_test
  ${CAD_CORE_SOURCES}
  tests/face_projection_arc_test.cpp
)

target_include_directories(cad_core_face_projection_arc_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_face_projection_arc_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

add_executable(cad_core_stl_writer_test
  ${CAD_CORE_SOURCES}
  tests/stl_writer_test.cpp
)

target_include_directories(cad_core_stl_writer_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_stl_writer_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

add_executable(cad_core_stl_import_test
  ${CAD_CORE_SOURCES}
  tests/stl_import_test.cpp
)

target_include_directories(cad_core_stl_import_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_stl_import_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

add_executable(cad_core_dxf_import_export_test
  ${CAD_CORE_SOURCES}
  tests/dxf_import_export_test.cpp
)

add_executable(cad_core_step_import_test
  ${CAD_CORE_SOURCES}
  tests/step_import_test.cpp
)

target_include_directories(cad_core_step_import_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_step_import_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

add_executable(cad_core_iges_import_export_test
  ${CAD_CORE_SOURCES}
  tests/iges_import_export_test.cpp
)

target_include_directories(cad_core_iges_import_export_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_iges_import_export_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

target_include_directories(cad_core_dxf_import_export_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_dxf_import_export_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)

# Text engine unit tests — links only the engine + OCCT font stack
# (no full CAD core), so font behavior is exercised in isolation.
add_executable(cad_core_text_engine_test
  tests/text_engine_test.cpp
  src/core/text_engine.cpp
  tests/logger_stub.cpp
)

target_include_directories(cad_core_text_engine_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
)

target_link_libraries(cad_core_text_engine_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
)

# Sketch text integration tests — drive the full DocumentManager like
# sketch_move_test: expansion, profiles, guards, extrude, save/load.
add_executable(cad_core_text_test
  ${CAD_CORE_SOURCES}
  tests/text_test.cpp
)

target_include_directories(cad_core_text_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${PLANEGCS_DIR}
  ${CMAKE_SOURCE_DIR}/native_compat
  ${PLANEGCS_DIR}/headers
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(cad_core_text_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
  planegcs
  dxfrw
)
