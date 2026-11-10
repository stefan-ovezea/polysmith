// Two defects behind one line of log noise, reported on every load:
//   "solver solution rejected (circle circle-104 collapsed from 7.758893 mm to
//    -8.132016 mm)"
// -8.132016 is circle-102's centre Y, i.e. a VERTEX COORDINATE was read as a
// radius. Two causes:
//
// 1. `save_document_to_path` serialized with the event payload's defaults, which
//    keep only the OPEN sketch's vertex table — a saved file came back with
//    circles referencing vertices that did not exist.
// 2. `apply()` read a circle's radius as `vertices.size() * 2 + index`, assuming
//    the radii are contiguous. The build pushes an anonymous centre (two
//    parameters) for a circle whose centre vertex is missing, which shifts every
//    later radius by two.
//
// The touch guard above the solver threw the bad solution away, so the sketch
// looked right and the warning returned on every solve. These two tests sit
// below and beside the guard: one reads the saved file, the other drives the
// solver on a sketch whose centres are deliberately missing.
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/document/document.h"
#include "core/document/feature.h"
#include "core/sketch/constraint_solver.h"
#include "core/sketch/sketch_feature_parameters.h"

namespace {

using polysmith::core::ConstraintSolver;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchCircle;
using polysmith::core::SketchFeatureParameters;

bool expect(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL: " << what << "\n";
    return false;
  }
  return true;
}

const SketchFeatureParameters* find_sketch(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      return &feature.sketch_parameters.value();
    }
  }
  return nullptr;
}

// The reporter's sketch: an outline, two overlapping circles, and a third
// concentric with the bigger one.
DocumentState build_reporter_sketch(DocumentManager& manager) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(-50.0, -35.0, 50.0, 35.0);
  manager.add_sketch_circle(14.46, -8.13, 16.57);
  manager.add_sketch_circle(-20.0, 10.0, 22.36);
  const DocumentState with_third = manager.add_sketch_circle(-20.0, 10.0, 7.7589);
  static_cast<void>(with_third);
  return manager.finish_sketch();
}

std::vector<double> radii_of(const SketchFeatureParameters& sketch) {
  std::vector<double> radii;
  for (const auto& circle : sketch.circles) {
    radii.push_back(circle.radius);
  }
  return radii;
}

bool test_a_saved_sketch_keeps_its_vertex_table() {
  DocumentManager manager;
  build_reporter_sketch(manager);

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "polysmith_saved_vertices.polysmith";
  manager.save_document_to_path(path.string());

  std::ifstream stream(path);
  if (!expect(stream.is_open(), "fixture: the file was written")) {
    return false;
  }
  nlohmann::json payload;
  stream >> payload;
  stream.close();
  std::filesystem::remove(path);

  const auto& history = payload.at("document").at("feature_history");
  for (const auto& feature : history) {
    if (feature.at("kind") != "sketch") {
      continue;
    }
    const auto& sketch = feature.at("sketch_parameters");
    const auto vertices = sketch.at("vertices");
    if (!expect(!vertices.empty(),
                "a saved sketch keeps its vertex table (regression: the file "
                "came back with every centre vertex missing)")) {
      return false;
    }
    // And the radii in the file are the radii the user drew.
    const auto& circles = sketch.at("circles");
    return expect(circles.size() == 3 &&
                      circles.at(2).at("radius").get<double>() > 7.0,
                  "the saved circles keep their radii");
  }
  return expect(false, "fixture: a sketch exists");
}

// Two circles with no vertex table at all: the solver has to invent a centre for
// each, and must still read each radius from its own slot.
bool test_a_circle_without_a_centre_vertex_keeps_its_radius() {
  SketchFeatureParameters params;
  const auto add_circle = [&params](const std::string& id, double x, double y,
                                    double r) {
    SketchCircle circle;
    circle.id = id;
    circle.center_vertex_id = "vertex-missing-" + id;
    circle.center_x = x;
    circle.center_y = y;
    circle.radius = r;
    params.circles.push_back(circle);
  };
  add_circle("circle-a", 0.0, 0.0, 16.57);
  add_circle("circle-b", 20.0, 0.0, 7.7589);

  ConstraintSolver solver;
  solver.build(params);
  const auto result = solver.solve();
  if (!expect(result.ok(), "fixture: the sketch solves")) {
    return false;
  }
  solver.apply(params);

  const std::vector<double> expected{16.57, 7.7589};
  for (size_t i = 0; i < params.circles.size() && i < expected.size(); ++i) {
    const double after = params.circles[i].radius;
    if (!expect(std::abs(after - expected[i]) <= 1e-3,
                "a circle without a centre vertex still reads its own radius "
                "(a centre coordinate read as a radius means the slots drifted)")) {
      std::cout << "  " << params.circles[i].id << " expected " << expected[i]
                << " got " << after << "\n";
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  if (!test_a_saved_sketch_keeps_its_vertex_table()) {
    return 1;
  }
  if (!test_a_circle_without_a_centre_vertex_keeps_its_radius()) {
    return 1;
  }
  std::cout << "solver_radius_mapping_test passed\n";
  return 0;
}
