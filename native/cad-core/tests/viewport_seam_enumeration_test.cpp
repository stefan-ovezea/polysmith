// Regression test for the viewport seam-topology enumeration rewrite.
//
// Before the rewrite, `enumerate_body_edges` probed EVERY face for
// EVERY edge (O(E×F)) to find seam lines, and
// `enumerate_body_vertices` rebuilt the body's edge map for EVERY
// vertex and then scanned all edges (O(V×E)) to find circle-seam
// vertices. A STEP-imported KiCad PCB assembly (~78k edges, ~156k
// vertices, ~15k faces) took ~20 minutes to appear in the viewport
// because of those two probes. The rewrite precomputes the seam
// topology per body in one incidence pass.
//
// This suite locks the SEMANTICS of the rewrite — which edges and
// vertices are excluded from the pickable sets — on canonical shapes,
// plus a many-solid tripwire so a reintroduced quadratic pass cannot
// ship unnoticed.
//
// Fixtures follow the in-repo convention: writer-generated STEP files
// (STEPControl_Writer), like the step/stl import suites.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/document/document.h"
#include "core/viewport/viewport.h"

namespace {

using namespace polysmith::core;
using Clock = std::chrono::steady_clock;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

std::string temp_step_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() /
          ("polysmith_seam_enum_" + name + ".step"))
      .string();
}

// Writes `shape` as a STEP file and returns the path.
std::string write_step(const std::string& name, const TopoDS_Shape& shape) {
  const std::string path = temp_step_path(name);
  STEPControl_Writer writer;
  if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Transfer failed\n";
    return "";
  }
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Write failed\n";
    return "";
  }
  return path;
}

// Imports the STEP at `path` into a fresh document and builds the
// viewport state — the exact pipeline the app runs after an import.
// `manager` must be shared across tests: the viewport rebuild caches
// in viewport.cpp are process-static and keyed by
// (document id, revision, timeline cursor, selections), and fresh
// managers restart the document-id counter — two tests with separate
// managers would collide on "doc-1" and serve each other's cached
// topology. One manager gives every test a distinct document id.
ViewportState import_and_build_viewport(DocumentManager& manager,
                                        const std::string& path) {
  manager.create_document();
  const DocumentState document = manager.import_step(path);
  return build_viewport_state(std::optional<DocumentState>(document));
}

// --- canonical-shape semantics ------------------------------------------

// A cylinder carries exactly the topology the seam rewrite must
// classify: the lateral face's closing line (the seam), two full-circle
// rim edges, and four vertices whose incident edges are all seam lines
// or full circles.
bool test_cylinder_seam_exclusion(DocumentManager& manager) {
  const std::string path = write_step(
      "cylinder", BRepPrimAPI_MakeCylinder(/*radius=*/10.0,
                                           /*height=*/30.0)
                      .Shape());

  const ViewportState viewport = import_and_build_viewport(manager, path);

  if (!expect(viewport.meshes.size() == 1 &&
                  !viewport.meshes.front().indices.empty(),
              "cylinder: the imported body must emit a renderable mesh")) {
    return false;
  }

  // 3 unique edges total: seam line + two rim circles. The seam line
  // closes the lateral face and must be excluded; both rims stay.
  if (!expect(viewport.edges.size() == 2,
              ("cylinder: expected 2 pickable edges (both rim circles, "
               "seam line excluded), got " +
               std::to_string(viewport.edges.size()))
                  .c_str())) {
    return false;
  }
  for (const auto& edge : viewport.edges) {
    if (!expect(edge.kind == "circle",
                "cylinder: remaining edges must be full circles")) {
      return false;
    }
    // The circle witness is what marks a full circle as a hole rim.
    if (!expect(edge.center.has_value() && edge.axis.has_value() &&
                    edge.radius.has_value(),
                "cylinder: full-circle edges must carry the circle witness")) {
      return false;
    }
    if (!expect(std::abs(edge.radius.value() - 10.0) < 1e-6,
                "cylinder: rim radius must match the cylinder")) {
      return false;
    }
  }

  // All four vertices are incident only to the seam line and/or the
  // full-circle rims — the rewrite excludes every one of them.
  return expect(viewport.vertices.empty(),
                ("cylinder: expected 0 pickable vertices, got " +
                 std::to_string(viewport.vertices.size()))
                    .c_str());
}

// A box has no seam lines and no full circles — everything stays.
bool test_box_emits_all_edges_and_vertices(DocumentManager& manager) {
  const std::string path = write_step(
      "box",
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -5.0), 40.0, 20.0, 10.0)
          .Shape());

  const ViewportState viewport = import_and_build_viewport(manager, path);

  return expect(viewport.edges.size() == 12,
                ("box: expected 12 pickable edges, got " +
                 std::to_string(viewport.edges.size()))
                    .c_str()) &&
         expect(viewport.vertices.size() == 8,
                ("box: expected 8 pickable vertices, got " +
                 std::to_string(viewport.vertices.size()))
                    .c_str()) &&
         expect(
             std::all_of(viewport.edges.begin(), viewport.edges.end(),
                         [](const auto& edge) { return edge.kind == "line"; }),
             "box: all emitted edges must be lines");
}

// --- many-solid tripwire -------------------------------------------------

// 600 disjoint boxes in one imported compound: 7200 edges, 4800
// vertices, 3600 faces. The old per-edge/per-vertex probes turn this
// into ~26M face probes + ~50M edge scans; the incidence pass is
// linear. The timing bound is deliberately loose so slow CI machines
// don't flake, but a reintroduced quadratic pass blows through it by
// an order of magnitude.
bool test_many_solid_compound_tripwire(DocumentManager& manager) {
  const std::string path = temp_step_path("many_boxes");
  STEPControl_Writer writer;
  constexpr int kBoxCount = 600;
  for (int i = 0; i < kBoxCount; ++i) {
    if (writer.Transfer(
            BRepPrimAPI_MakeBox(gp_Pnt(static_cast<double>(i) * 20.0, 0.0, 0.0),
                                10.0, 10.0, 10.0)
                .Shape(),
            STEPControl_AsIs) != IFSelect_RetDone) {
      std::cerr << "fixture: transfer of box " << i << " failed\n";
      return false;
    }
  }
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Write failed\n";
    return false;
  }

  const auto start = Clock::now();
  const ViewportState viewport = import_and_build_viewport(manager, path);
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            start)
          .count();

  constexpr int kExpectedEdges = kBoxCount * 12;
  constexpr int kExpectedVertices = kBoxCount * 8;
  if (!expect(viewport.edges.size() == kExpectedEdges,
              ("tripwire: expected " + std::to_string(kExpectedEdges) +
               " edges, got " + std::to_string(viewport.edges.size()))
                  .c_str()) ||
      !expect(viewport.vertices.size() == kExpectedVertices,
              ("tripwire: expected " + std::to_string(kExpectedVertices) +
               " vertices, got " + std::to_string(viewport.vertices.size()))
                  .c_str())) {
    return false;
  }

  if (elapsed_ms > 40000) {
    std::cerr << "tripwire: import + viewport rebuild took " << elapsed_ms
              << " ms (> 40 s) — the edge/vertex enumeration may have "
                 "regressed to the old quadratic probes\n";
    return false;
  }
  return true;
}

// --- oversized-import pick budget ----------------------------------------

// Mirrors kMaxMeshFacePickTriangles in
// core/viewport/impl/body_face_helpers.inc (not a header — duplicated
// here like kMaxImportPickEdgeCount below).
constexpr int kTestPickFaceTriangleBudget = 48;

// A 100-gon prism: both end faces are planar with a 100-vertex outer
// wire, which the mesher splits into ~98 triangles — above the pick
// budget, so they exercise the bounded centroid-fan proxy path.
TopoDS_Shape make_ngon_prism(int sides, double radius, double height) {
  const double kPi = std::acos(-1.0);
  BRepBuilderAPI_MakeWire wire_builder;
  for (int i = 0; i < sides; ++i) {
    const double start_angle = 2.0 * kPi * static_cast<double>(i) / sides;
    const double end_angle = start_angle + 2.0 * kPi / sides;
    wire_builder.Add(BRepBuilderAPI_MakeEdge(
        gp_Pnt(radius * std::cos(start_angle),
               radius * std::sin(start_angle), 0.0),
        gp_Pnt(radius * std::cos(end_angle), radius * std::sin(end_angle),
               0.0)));
  }
  if (!wire_builder.IsDone()) {
    std::cerr << "fixture: ngon wire failed\n";
    return TopoDS_Shape();
  }
  BRepBuilderAPI_MakeFace face_builder(wire_builder.Wire());
  if (!face_builder.IsDone()) {
    std::cerr << "fixture: ngon face failed\n";
    return TopoDS_Shape();
  }
  BRepPrimAPI_MakePrism prism(face_builder.Face(), gp_Vec(0.0, 0.0, height));
  if (!prism.IsDone()) {
    std::cerr << "fixture: ngon prism failed\n";
    return TopoDS_Shape();
  }
  return prism.Shape();
}

// Imports above kMaxImportPickEdgeCount (8000) edges skip the per-edge
// and per-vertex pick entries — they would flood the viewport payload
// and the scene-graph (see viewport.cpp) — but still emit a DECIMATED
// per-face pick proxy per face so sketch-on-face placement, face
// selection, and face-based features keep working. Faces above the
// pick-triangle budget are fanned (planar) or strided (curved) down to
// kMaxMeshFacePickTriangles; small faces ship their full (tiny)
// triangulation.
bool test_oversized_import_decimated_face_proxies(DocumentManager& manager) {
  const std::string path = temp_step_path("huge_import");
  STEPControl_Writer writer;
  // 900 boxes + one big cylinder + one 100-gon prism: 10800 + 3 + 300
  // edges > kMaxImportPickEdgeCount (8000). The cylinder's lateral
  // face meshes to ~1440 triangles (deflection 0.1, angular 0.5°) so
  // the strided (curved-face) path is exercised, and the prism end
  // faces pin the bounded fan path.
  constexpr int kBoxCount = 900;
  for (int i = 0; i < kBoxCount; ++i) {
    if (writer.Transfer(
            BRepPrimAPI_MakeBox(gp_Pnt(static_cast<double>(i) * 20.0, 0.0, 0.0),
                                10.0, 10.0, 10.0)
                .Shape(),
            STEPControl_AsIs) != IFSelect_RetDone) {
      std::cerr << "fixture: transfer of box " << i << " failed\n";
      return false;
    }
  }
  if (writer.Transfer(BRepPrimAPI_MakeCylinder(/*radius=*/50.0,
                                               /*height=*/100.0)
                          .Shape(),
                      STEPControl_AsIs) != IFSelect_RetDone) {
    std::cerr << "fixture: transfer of cylinder failed\n";
    return false;
  }
  if (writer.Transfer(make_ngon_prism(/*sides=*/100, /*radius=*/200.0,
                                      /*height=*/20.0),
                      STEPControl_AsIs) != IFSelect_RetDone) {
    std::cerr << "fixture: transfer of ngon prism failed\n";
    return false;
  }
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Write failed\n";
    return false;
  }

  const ViewportState viewport = import_and_build_viewport(manager, path);

  if (!expect(viewport.meshes.size() == 1 &&
                  !viewport.meshes.front().indices.empty(),
              "oversized: the body mesh must still be emitted")) {
    return false;
  }
  if (!expect(viewport.edges.empty(),
              "oversized: per-edge pick entries must be skipped") ||
      !expect(viewport.vertices.empty(),
              "oversized: per-vertex pick entries must be skipped") ||
      !expect(viewport.bodies.size() == 1,
              "oversized: the body summary must still be emitted")) {
    return false;
  }

  // 900 boxes (6 faces) + cylinder (2 rims + 1 wall) + 100-gon prism
  // (100 sides + 2 end faces) — every face gets a pick proxy.
  constexpr int kExpectedFaces = kBoxCount * 6 + 3 + 102;
  if (!expect(viewport.solid_faces.size() == kExpectedFaces,
              ("oversized: expected " + std::to_string(kExpectedFaces) +
               " face proxies, got " +
               std::to_string(viewport.solid_faces.size()))
                  .c_str())) {
    return false;
  }

  bool found_planar_face = false;
  bool found_capped_fan_face = false;
  bool found_cylinder_face = false;
  for (const auto& face : viewport.solid_faces) {
    if (face.triangle_indices.size() % 3 != 0 ||
        static_cast<int>(face.triangle_indices.size() / 3) >
            kTestPickFaceTriangleBudget) {
      std::cerr << "oversized: face proxy " << face.face_id << " ships "
                << face.triangle_indices.size() / 3
                << " triangles — above the pick budget\n";
      return false;
    }
    if (face.sketchability == "planar") {
      found_planar_face = true;
      // The prism end faces are planar with 100-edge outer wires: a
      // bounded fan (~34 triangles). Without the boundary cap the fan
      // would ship 100 triangles and trip the budget check above;
      // without decimation the mesher's ~98 triangles would do the
      // same. A count above 2 (a box face's full pair) pins the
      // decimated fan specifically.
      if (face.triangle_indices.size() / 3 > 2) {
        found_capped_fan_face = true;
      }
    }
    if (face.surface_kind == "cylinder" &&
        face.cylinder_radius.has_value()) {
      found_cylinder_face = true;
    }
  }

  return expect(found_planar_face,
                "oversized: planar face proxies must be emitted (sketch "
                "on face placement)") &&
         expect(found_capped_fan_face,
                "oversized: a decimated planar fan proxy must exist (the "
                "100-gon prism end faces)") &&
         expect(found_cylinder_face,
                "oversized: curved faces keep their surface kind + radius "
                "witness (the cylinder wall)");
}

}  // namespace

int main() {
  DocumentManager manager;
  std::cerr << "[viewport_seam_enumeration_test] test 1: cylinder seam\n";
  if (!test_cylinder_seam_exclusion(manager)) return 1;
  std::cerr << "[viewport_seam_enumeration_test] test 2: box keeps all\n";
  if (!test_box_emits_all_edges_and_vertices(manager)) return 1;
  std::cerr << "[viewport_seam_enumeration_test] test 3: many-solid tripwire\n";
  if (!test_many_solid_compound_tripwire(manager)) return 1;
  std::cerr << "[viewport_seam_enumeration_test] test 4: oversized import "
               "face proxies\n";
  if (!test_oversized_import_decimated_face_proxies(manager)) return 1;
  std::cout << "viewport_seam_enumeration_test passed\n";
  return 0;
}
