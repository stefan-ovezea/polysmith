// Regression tests for the sketch spline entity (feature/sketch).
//
// v1: control-point creation — the user's clicks ARE the B-spline
// poles (regular movable vertices).  The drawn curve is the clamped
// degree-3 B-spline they define (spline_math.h); there is no solver
// registration, so pole drags re-fit the curve through the ordinary
// vertex sync.  The profile engine treats the spline as an open exact
// curve (entity_kind "spline"); intersections delegate to OCCT 2D
// algorithms (spline_profile_occt.cpp).  The wire builder emits the
// exact Geom_BSplineCurve edge.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
#include "core/sketch/spline_math.h"
#include "protocol/serialization.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchSpline;
using polysmith::test::ExpectedProfile;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

// 4 poles: (0,0) (4,6) (8,6) (12,0) — a symmetric bulge.
const std::vector<std::pair<double, double>> kBulgePoles = {
    {0.0, 0.0}, {4.0, 6.0}, {8.0, 6.0}, {12.0, 0.0}};

// Evaluates the document's first spline at u via the same math the
// profile walk uses.
std::pair<double, double> spline_point_at(const DocumentState& document,
                                          double u) {
  const auto params = sketch_params(document);
  const SketchSpline& s = params.splines[0];
  const auto knots =
      polysmith::core::spline_open_uniform_knots(
          static_cast<int>(s.pole_xs.size()), s.degree);
  const polysmith::core::SplineSample sample = polysmith::core::spline_eval(
      s.degree, knots, s.pole_xs, s.pole_ys, u);
  return {sample.x, sample.y};
}

bool test_spline_creation() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  const auto params = sketch_params(document);
  if (!expect(params.splines.size() == 1, "spline: entity created")) {
    return false;
  }
  const SketchSpline& s = params.splines[0];
  if (!expect(s.pole_xs.size() == 4 && s.pole_ys.size() == 4,
              "spline: 4 poles cached")) {
    return false;
  }
  if (!expect(s.degree == 3, "spline: degree = min(3, n-1)")) {
    return false;
  }
  if (!expect(s.pole_vertex_ids.size() == 4,
              "spline: one vertex per pole")) {
    return false;
  }
  // Poles are regular movable vertices (control-point spline).
  bool all_poles_movable = true;
  for (const auto& vid : s.pole_vertex_ids) {
    const auto vertex_it = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& v) { return v.id == vid; });
    if (vertex_it == params.vertices.end() || vertex_it->is_fixed) {
      all_poles_movable = false;
    }
  }
  if (!expect(all_poles_movable, "spline: poles are movable vertices")) {
    return false;
  }
  // The curve passes through the first and last poles exactly.
  const auto start = spline_point_at(document, 0.0);
  const auto end = spline_point_at(document, 1.0);
  return expect(near(start.first, 0.0) && near(start.second, 0.0) &&
                    near(end.first, 12.0) && near(end.second, 0.0),
                "spline: curve endpoints land on the end poles");
}

bool test_spline_pole_count_validation() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  bool threw = false;
  try {
    (void)manager.add_sketch_spline({{1.0, 1.0}});
  } catch (const std::exception&) {
    threw = true;
  }
  if (!expect(threw, "spline: a single pole is rejected")) {
    return false;
  }

  // Two poles: degree clamps to 1 (a straight line through both).
  DocumentState document =
      manager.add_sketch_spline({{0.0, 0.0}, {10.0, 0.0}});
  const auto params = sketch_params(document);
  return expect(params.splines[0].degree == 1,
                "spline: two poles give degree 1");
}

bool test_spline_pole_drag_refits() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  const auto before = sketch_params(document);
  const SketchSpline& s = before.splines[0];

  // Drag the second pole (the movable interior control point) up and
  // right.  The curve must re-fit: the midpoint rises, the endpoints
  // stay exactly put (clamped B-spline).
  const auto mid_before = spline_point_at(document, 0.5);
  document = manager.update_sketch_vertex(s.pole_vertex_ids[1], 6.0, 10.0);

  const auto after = sketch_params(document);
  if (!expect(near(after.splines[0].pole_xs[1], 6.0) &&
                  near(after.splines[0].pole_ys[1], 10.0),
              "spline: pole cache follows the vertex drag")) {
    return false;
  }
  const auto mid_after = spline_point_at(document, 0.5);
  const auto start_after = spline_point_at(document, 0.0);
  const auto end_after = spline_point_at(document, 1.0);
  if (!expect(mid_after.second > mid_before.second,
              "spline: dragged pole reshapes the curve")) {
    return false;
  }
  return expect(near(start_after.first, 0.0) && near(start_after.second, 0.0) &&
                    near(end_after.first, 12.0) && near(end_after.second, 0.0),
                "spline: endpoints stay anchored through the drag");
}

bool test_spline_region_full_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Spline bulge + two closing lines = one bounded region.
  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  document = manager.add_sketch_line(12.0, 0.0, 6.0, -3.0);
  document = manager.add_sketch_line(6.0, -3.0, 0.0, 0.0);

  const auto params = sketch_params(document);
  const std::string spline_id = params.splines[0].id;
  const auto lines = params.lines;
  if (!expect(lines.size() == 2, "spline: closing lines created")) {
    return false;
  }

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {spline_id, lines[0].id, lines[1].id},
       .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("spline region: " + reason).c_str())) {
    return false;
  }

  // The region carries an exact spline boundary edge with the poles.
  const auto& profile = params.profiles[0];
  bool has_spline_edge = false;
  for (const auto& be : profile.boundary_edges) {
    if (be.entity_kind == "spline") {
      has_spline_edge = true;
      if (!expect(be.spline_pole_xs.size() == 4 &&
                      near(be.spline_pole_xs[1], 4.0) &&
                      near(be.spline_pole_ys[1], 6.0),
                  "spline: boundary edge carries the control poles")) {
        return false;
      }
    }
  }
  return expect(has_spline_edge,
                "spline: profile carries an exact spline boundary edge");
}

bool test_spline_closed_region() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // A closed control polygon: the last pole coincides with the first.
  // The closed spline must bound a region BY ITSELF (like a full
  // circle) — the user's "click the first pole to close" gesture.
  const std::vector<std::pair<double, double>> closed_poles = {
      {0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}, {0.0, 0.0}};
  DocumentState document = manager.add_sketch_spline(closed_poles);
  const auto params = sketch_params(document);
  const std::string spline_id = params.splines[0].id;

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {spline_id}, .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("spline closed region: " + reason).c_str())) {
    return false;
  }

  // The region carries the full-span spline boundary edge.
  const auto& profile = params.profiles[0];
  bool has_spline_edge = false;
  for (const auto& be : profile.boundary_edges) {
    if (be.entity_kind == "spline") {
      has_spline_edge = true;
      if (!expect(be.spline_pole_xs.size() == 5,
                  "spline: closed boundary edge carries all poles")) {
        return false;
      }
    }
  }
  return expect(has_spline_edge,
                "spline: closed region carries the spline boundary edge");
}

bool test_spline_closed_extrude_smoke() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const std::vector<std::pair<double, double>> closed_poles = {
      {0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}, {0.0, 0.0}};
  DocumentState document = manager.add_sketch_spline(closed_poles);
  const auto params = sketch_params(document);
  const std::string profile_id = params.profiles[0].id;

  document = manager.extrude_profile(profile_id, 5.0, "new_body");
  const auto compiled = polysmith::core::compile_bodies(document);
  return expect(compiled.bodies.size() == 1,
                "spline: closed spline extrudes to a body");
}

bool test_spline_region_split_by_connectors() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // The bulge region, split by two connectors whose tops land EXACTLY
  // on the spline (u = 0.25 and 0.75 of the clamped degree-3 bulge:
  // x = 3 / 9, y = 3.375) and whose bottoms land on the closing line.
  // Three regions with DISTINCT entity-id sets — the complete-set
  // assertion below is the regression net for the spline walk.
  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  document = manager.add_sketch_line(12.0, 0.0, 0.0, 0.0);
  document = manager.add_sketch_line(3.0, 0.0, 3.0, 3.375);
  document = manager.add_sketch_line(9.0, 0.0, 9.0, 3.375);

  const auto params = sketch_params(document);
  const std::string spline_id = params.splines[0].id;
  const std::vector<std::string> ids = {
      spline_id,
      params.lines[0].id,  // closing line
      params.lines[1].id,  // left connector
      params.lines[2].id,  // right connector
  };

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {ids[0], ids[1], ids[2]}, .kind = "polygon"},
      {.entity_ids = {ids[0], ids[1], ids[3]}, .kind = "polygon"},
      {.entity_ids = {ids[0], ids[1], ids[2], ids[3]}, .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("spline split: " + reason).c_str())) {
    return false;
  }
  bool spline_walked = false;
  for (const auto& profile : params.profiles) {
    for (const auto& be : profile.boundary_edges) {
      if (be.entity_kind == "spline") spline_walked = true;
    }
  }
  return expect(spline_walked,
                "spline: sub-regions traverse spline edges");
}

bool test_spline_dangling_crossing_line_dropped() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // A vertical line CROSSING the bulge (exercising the OCCT spline x
  // line intersection path) but connected to nothing at its ends: the
  // dangling-drop rule removes it and the region stays whole — a
  // dead-end line must never split a profile (the trim regression).
  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  document = manager.add_sketch_line(12.0, 0.0, 0.0, 0.0);
  document = manager.add_sketch_line(5.0, -2.0, 5.0, 6.0);

  const auto params = sketch_params(document);
  return expect(params.profiles.size() == 1,
                "spline: unconnected crossing line does not split the region");
}

bool test_spline_extrude_smoke() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  document = manager.add_sketch_line(12.0, 0.0, 6.0, -3.0);
  document = manager.add_sketch_line(6.0, -3.0, 0.0, 0.0);
  const auto params = sketch_params(document);
  const std::string profile_id = params.profiles[0].id;

  document = manager.extrude_profile(profile_id, 5.0, "new_body");

  bool has_extrude = false;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "extrude") has_extrude = true;
  }
  const auto compiled = polysmith::core::compile_bodies(document);
  if (!expect(has_extrude && compiled.bodies.size() == 1,
              "spline: extrude produces a body from the spline profile")) {
    return false;
  }

  // The extruded spline region must survive save/load: the boundary
  // edge's control poles round-trip through the extrude serializer.
  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_spline_extrude_test.polysmith")
          .string();
  {
    std::ofstream stream(path);
    stream << polysmith::protocol::to_payload(document).dump();
  }
  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(path);
  const auto loaded_compiled = polysmith::core::compile_bodies(loaded);
  return expect(loaded_compiled.bodies.size() == 1,
                "spline: extruded spline profile survives save/load");
}

bool test_spline_move_translates_poles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  const std::string spline_id = sketch_params(document).splines[0].id;
  const auto mid_before = spline_point_at(document, 0.5);

  document = manager.move_sketch_entities({spline_id}, 5.0, 5.0, 0.0, 0.0,
                                          0.0);
  const auto after = sketch_params(document);
  const SketchSpline& s = after.splines[0];
  if (!expect(near(s.pole_xs[0], 5.0) && near(s.pole_ys[0], 5.0) &&
                  near(s.pole_xs[3], 17.0) && near(s.pole_ys[3], 5.0),
              "spline: all poles translate with the move")) {
    return false;
  }
  const auto mid_after = spline_point_at(document, 0.5);
  return expect(near(mid_after.first, mid_before.first + 5.0) &&
                    near(mid_after.second, mid_before.second + 5.0),
                "spline: curve shape is preserved by the rigid move");
}

bool test_spline_trim_rejected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_spline(kBulgePoles);
  const std::string spline_id = sketch_params(document).splines[0].id;

  // The trim engine handles line/circle/arc only — a spline id must
  // be rejected (v1: explicit errors, matching the ellipse contract).
  bool threw = false;
  try {
    (void)manager.trim_sketch_entity(spline_id, 4.0, 4.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "spline: trim on a spline is rejected");
}

bool test_spline_construction_and_save_roundtrip() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_spline(kBulgePoles, true);
  const auto params = sketch_params(document);
  if (!expect(params.splines[0].is_construction,
              "spline: construction flag set")) {
    return false;
  }
  if (!expect(params.profiles.empty(),
              "spline: construction spline contributes no profile")) {
    return false;
  }

  // Save / load round-trip: poles, degree and the construction flag
  // all survive.
  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_spline_test.polysmith")
          .string();
  {
    std::ofstream stream(path);
    stream << polysmith::protocol::to_payload(document).dump();
  }
  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(path);
  const auto loaded_params = sketch_params(loaded);
  if (!expect(loaded_params.splines.size() == 1,
              "spline: survives save/load")) {
    return false;
  }
  const SketchSpline& s = loaded_params.splines[0];
  return expect(s.degree == 3 && s.is_construction &&
                    s.pole_xs.size() == 4 &&
                    near(s.pole_xs[1], 4.0) && near(s.pole_ys[1], 6.0) &&
                    s.pole_vertex_ids.size() == 4,
                "spline: poles/degree/flag round-trip through save/load");
}

}  // namespace

int main() {
  try {
  std::cout << "--- test_spline_creation" << std::endl;
  if (!test_spline_creation()) return 1;
  std::cout << "--- test_spline_pole_count_validation" << std::endl;
  if (!test_spline_pole_count_validation()) return 1;
  std::cout << "--- test_spline_pole_drag_refits" << std::endl;
  if (!test_spline_pole_drag_refits()) return 1;
  std::cout << "--- test_spline_region_full_profile" << std::endl;
  if (!test_spline_region_full_profile()) return 1;
  std::cout << "--- test_spline_closed_region" << std::endl;
  if (!test_spline_closed_region()) return 1;
  std::cout << "--- test_spline_closed_extrude_smoke" << std::endl;
  if (!test_spline_closed_extrude_smoke()) return 1;
  std::cout << "--- test_spline_region_split_by_connectors" << std::endl;
  if (!test_spline_region_split_by_connectors()) return 1;
  std::cout << "--- test_spline_dangling_crossing_line_dropped" << std::endl;
  if (!test_spline_dangling_crossing_line_dropped()) return 1;
  std::cout << "--- test_spline_extrude_smoke" << std::endl;
  if (!test_spline_extrude_smoke()) return 1;
  std::cout << "--- test_spline_move_translates_poles" << std::endl;
  if (!test_spline_move_translates_poles()) return 1;
  std::cout << "--- test_spline_trim_rejected" << std::endl;
  if (!test_spline_trim_rejected()) return 1;
  std::cout << "--- test_spline_construction_and_save_roundtrip" << std::endl;
  if (!test_spline_construction_and_save_roundtrip()) return 1;

  std::cout << "spline_test passed\n";
  return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
