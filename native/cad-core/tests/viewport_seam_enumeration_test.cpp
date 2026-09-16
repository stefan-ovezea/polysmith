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

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

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

// Imports above kMaxImportPickEdgeCount edges emit ONLY the body mesh
// primitive (body-level picking), mirroring mesh_import — the per-edge/
// per-vertex/per-face entries would otherwise flood the viewport
// payload and the scene-graph (see viewport.cpp).
bool test_oversized_import_body_level_picking(DocumentManager& manager) {
  const std::string path = temp_step_path("huge_import");
  STEPControl_Writer writer;
  // 900 boxes = 10800 edges > kMaxImportPickEdgeCount (8000).
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
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Write failed\n";
    return false;
  }

  const ViewportState viewport = import_and_build_viewport(manager, path);

  if (!expect(viewport.meshes.size() == 1 &&
                  !viewport.meshes.front().indices.empty(),
              "oversized: the body mesh must still be emitted (the only "
              "renderable primitive)")) {
    return false;
  }
  return expect(viewport.edges.empty(),
                "oversized: per-edge pick entries must be skipped") &&
         expect(viewport.vertices.empty(),
                "oversized: per-vertex pick entries must be skipped") &&
         expect(viewport.solid_faces.empty(),
                "oversized: per-face pick entries must be skipped") &&
         expect(viewport.bodies.size() == 1,
                "oversized: the body summary must still be emitted");
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
  std::cerr << "[viewport_seam_enumeration_test] test 4: oversized import\n";
  if (!test_oversized_import_body_level_picking(manager)) return 1;
  std::cout << "viewport_seam_enumeration_test passed\n";
  return 0;
}
