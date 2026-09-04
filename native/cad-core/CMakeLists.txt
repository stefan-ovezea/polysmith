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
  # Parallel per-TU compilation via MSBuild's multi-tool task scheduler:
  # one cl.exe per TU, total process count bounded by the /m value that
  # `cmake --build --parallel N` passes.  Do NOT combine with /MP — MTT
  # disables it, and cmake --build injects CL_MPCount=1 anyway.  Requires
  # MSBuild 16.4+ (VS 2019 16.4+); consumed only by the VS generator and
  # silently ignored by Makefiles/Ninja.
  set(CMAKE_VS_GLOBALS
    "UseMultiToolTask=true"
    "EnforceProcessCountAcrossBuilds=true"
  )
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
  src/core/cam/cam2d.cpp
  src/core/cam/cam_export.cpp
  src/core/cam/cam_generate.cpp
  src/core/cam/cam_generators.cpp
  src/core/cam/cam_operation.cpp
  src/core/cam/cam_planning.cpp
  src/core/cam/cam_profile_reference.cpp
  src/core/cam/cam_refresh.cpp
  src/core/cam/cam_resolution.cpp
  src/core/cam/cam_runtime.cpp
  src/core/cam/face_milling.cpp
  src/core/cam/laser/laser_fill.cpp
  src/core/cam/laser/laser_generate.cpp
  src/core/cam/laser/laser_leads.cpp
  src/core/cam/laser/laser_order.cpp
  src/core/cam/laser/laser_tabs.cpp
  src/core/cam/laser/laser_test_pattern.cpp
  src/core/cam/laser_cut.cpp
  src/core/cam/machine_library.cpp
  src/core/cam/post_processor.cpp
  src/core/cam/toolpath_geometry.cpp
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
  src/core/sketch/sketch_curve.cpp
  src/core/sketch/spline_profile_occt.cpp
  src/core/sketch/trim_engine.cpp
  src/core/sweep/sweep_feature.cpp
  src/core/text_engine.cpp
  src/core/viewport/viewport.cpp
  src/protocol/ipc.cpp
  src/protocol/serialization.cpp
)

# ---- CAD core static library ------------------------------------------------
# All full-core test suites and the cad_core exe link this library instead of
# recompiling every source file per target (~35x fewer TU compilations per
# clean build).  Static-lib link semantics pull only referenced objects, so
# any future TU that self-registers via a static initializer would be
# silently dropped here — registration must stay explicit.
add_library(cad_core_lib STATIC ${CAD_CORE_SOURCES})

target_include_directories(cad_core_lib PUBLIC
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
)

target_link_libraries(cad_core_lib PUBLIC ${OpenCASCADE_LIBRARIES} planegcs dxfrw)

add_executable(cad_core
  src/app.cpp
  src/main.cpp
)

target_link_libraries(cad_core PRIVATE cad_core_lib)

# Tauri looks for cad_core.exe in build/ (not build/Release/).
add_custom_command(TARGET cad_core POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:cad_core>
    ${CMAKE_BINARY_DIR}/$<TARGET_FILE_NAME:cad_core>
  COMMENT "Copying cad_core to build/ for Tauri"
)

# Full-core test suite: links cad_core_lib instead of recompiling
# ${CAD_CORE_SOURCES} per target.  The include list matches the old
# per-target block verbatim to preserve include order exactly.
function(polysmith_add_core_test NAME TEST_FILE)
  add_executable(${NAME} ${TEST_FILE})
  target_include_directories(${NAME} PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/../../third_party
    ${PLANEGCS_DIR}
    ${CMAKE_SOURCE_DIR}/native_compat
    ${PLANEGCS_DIR}/headers
    ${Boost_INCLUDE_DIRS}
  )
  target_link_libraries(${NAME} PRIVATE cad_core_lib)
endfunction()

add_executable(cad_core_cam2d_test
  # cam2d + cam_planning only — no cad_core / protocol dependency, so
  # no logger stub needed.  cam_planning (wire sampling) links OCCT.
  src/core/cam/cam2d.cpp
  src/core/cam/cam_planning.cpp
  tests/cam2d_test.cpp
)

target_include_directories(cad_core_cam2d_test PRIVATE
  src
  ${CMAKE_SOURCE_DIR}/../../third_party
  ${CMAKE_SOURCE_DIR}/native_compat
  ${OCCT_ROOT}/inc
)

target_link_libraries(cad_core_cam2d_test PRIVATE
  ${OpenCASCADE_LIBRARIES}
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
  src/core/sketch/sketch_curve.cpp
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

polysmith_add_core_test(cad_core_multi_profile_extrude_test tests/multi_profile_extrude_test.cpp)

polysmith_add_core_test(cad_core_cam_face_reference_test tests/cam_face_reference_test.cpp)

polysmith_add_core_test(cad_core_cam_save_load_test tests/cam_save_load_test.cpp)

polysmith_add_core_test(cad_core_cam_commands_test tests/cam_commands_test.cpp)

polysmith_add_core_test(cad_core_cam_machine_library_test tests/cam_machine_library_test.cpp)

polysmith_add_core_test(cad_core_cam_profile_reference_test tests/cam_profile_reference_test.cpp)

polysmith_add_core_test(cad_core_cam_refresh_test tests/cam_refresh_test.cpp)

polysmith_add_core_test(cad_core_grbl_post_test tests/grbl_post_test.cpp)

polysmith_add_core_test(cad_core_cam_generators_test tests/cam_generators_test.cpp)

polysmith_add_core_test(cad_core_plugin_feature_test tests/plugin_feature_test.cpp)

polysmith_add_core_test(cad_core_extrude_quality_test tests/extrude_quality_test.cpp)

polysmith_add_core_test(cad_core_sketch_move_test tests/sketch_move_test.cpp)

# Ellipse entity regression suite — creation, exact profile edge,
# extrude smoke, move preservation, construction exclusion.
polysmith_add_core_test(cad_core_ellipse_test tests/ellipse_test.cpp)

# Spline entity regression suite — control-point creation, pole drag
# re-fit, exact profile set, extrude smoke, move, trim rejection,
# save/load round-trip.
polysmith_add_core_test(cad_core_spline_test tests/spline_test.cpp)

# Slot entity regression suite — expansion to 2 lines + 2 arcs,
# exact profile set, param update re-expansion, generated-entity
# guards, move, extrude smoke.
polysmith_add_core_test(cad_core_slot_test tests/slot_test.cpp)

# Sketch chamfer regression suite — symmetric/asymmetric chamfers,
# distance edits (both epsilon sides), delete restores the corner,
# full profile set on a chamfered rectangle, fillet conflict.
polysmith_add_core_test(cad_core_sketch_chamfer_test tests/sketch_chamfer_test.cpp)

# Sketch extend regression suite — line/arc extension both directions,
# closed-loop profile after extend, generated-entity rejection,
# constraint preservation, arc angle dim flips to driven.
polysmith_add_core_test(cad_core_extend_test tests/extend_test.cpp)

# Sketch offset regression suite — line/circle/arc offsets on both
# distance signs, sweep preservation, collapse/invert rejection,
# construction/generated/unsupported rejection.
polysmith_add_core_test(cad_core_offset_test tests/offset_test.cpp)

# Sketch transform regression suite — in-place scale (H/V preserved,
# radii scaled, dims flipped/re-measured), exploded copies (fresh
# ids, vertex topology preserved among copies, H/V stripped on
# rotated copies), single-undo, two-profile copy.
polysmith_add_core_test(cad_core_transform_test tests/transform_test.cpp)

# Circle creation-mode regression suite — two/three-point circles,
# tangent-to-lines with circle-slave radius re-derivation, incenter,
# both wedge sides, full profile set.
polysmith_add_core_test(cad_core_circle_modes_test tests/circle_modes_test.cpp)

# Dimension-completion regression suite — diameter display, arc
# angle sweeps, arc length driving.
polysmith_add_core_test(cad_core_dimension_completion_test tests/dimension_completion_test.cpp)

# Sketch array regression suite — linear/circular exploded copies,
# unique ids, on-circle placement (1e-6), single-undo removal.
polysmith_add_core_test(cad_core_array_test tests/array_test.cpp)

# Intersecting-arcs extrude regression — the lobe between two arcs
# must cut its bottom-cap hole, not wire it as a giant complement
# region (the "thin wall" bug, res/part.json geometry).
polysmith_add_core_test(cad_core_intersecting_arcs_extrude_test tests/intersecting_arcs_extrude_test.cpp)

# Trim regression suite — the hovered segment index must decide the
# trim (not a re-derived click segment), and the arc-aware distance
# metric must replace the chord fallback for long arc segments.
polysmith_add_core_test(cad_core_trim_test tests/trim_test.cpp)

# Constraint-completion regression suite — symmetric, collinear,
# midpoint, tangent pairs, and parametric anchor-t enforcement.
polysmith_add_core_test(cad_core_constraint_completion_test tests/constraint_completion_test.cpp)

# Parametric arc regression suite — GCS::Arc solver integration: driving
# radius/angle dimensions, shared-endpoint drag, over-constraint fallback,
# complete profile sets for mixed line+arc loops.
polysmith_add_core_test(cad_core_parametric_arc_test tests/parametric_arc_test.cpp)

# Sketch tool whitelist regression suite — the two sketch-tool whitelists
# (validate_tool / is_supported_sketch_tool) must accept identical ids,
# and commands.schema.json must match the real command vocabulary.
polysmith_add_core_test(cad_core_tool_whitelist_test tests/tool_whitelist_test.cpp)

# Selection/highlight regression suite — stale selection clears and the
# central prune pass (feature/highlight).
polysmith_add_core_test(cad_core_selection_test tests/cad_core_selection_test.cpp)

# Face-projection arc recovery regression suite (feature/projection-arcs).
polysmith_add_core_test(cad_core_face_projection_arc_test tests/face_projection_arc_test.cpp)

polysmith_add_core_test(cad_core_stl_writer_test tests/stl_writer_test.cpp)

polysmith_add_core_test(cad_core_stl_import_test tests/stl_import_test.cpp)

polysmith_add_core_test(cad_core_dxf_import_export_test tests/dxf_import_export_test.cpp)

polysmith_add_core_test(cad_core_step_import_test tests/step_import_test.cpp)

polysmith_add_core_test(cad_core_iges_import_export_test tests/iges_import_export_test.cpp)

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
polysmith_add_core_test(cad_core_text_test tests/text_test.cpp)
