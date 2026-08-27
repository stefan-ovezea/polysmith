// Regression tests for the sketch ellipse entity (feature/sketch).
//
// v1: center + major-axis point + minor-axis point creation; the axis
// points are fixed at creation (no solver registration yet).  The
// ellipse participates in the exact profile engine as a full closed
// curve (entity_kind "ellipse") and the exact wire builder emits an
// analytic OCCT ellipse edge.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchEllipse;
using polysmith::core::SketchFeatureParameters;
using polysmith::test::ExpectedProfile;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

constexpr double kPi = 3.14159265358979323846;

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

bool test_ellipse_creation() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Center (10,10), major axis to (20,10) -> a=10 rotation=0, minor
  // axis click (10,16) -> b=6.
  DocumentState document =
      manager.add_sketch_ellipse(10.0, 10.0, 20.0, 10.0, 10.0, 16.0);
  const auto params = sketch_params(document);
  if (!expect(params.ellipses.size() == 1, "ellipse: entity created")) {
    return false;
  }
  const SketchEllipse& e = params.ellipses[0];
  if (!expect(near(e.center_x, 10.0) && near(e.center_y, 10.0),
              "ellipse: center cached")) {
    return false;
  }
  if (!expect(near(e.a, 10.0) && near(e.b, 6.0) && near(e.rotation, 0.0),
              "ellipse: a/b/rotation cached")) {
    return false;
  }
  // Axis points are fixed at creation.
  bool axis_a_fixed = false;
  bool axis_b_fixed = false;
  for (const auto& vertex : params.vertices) {
    if (vertex.id == e.axis_a_vertex_id) axis_a_fixed = vertex.is_fixed;
    if (vertex.id == e.axis_b_vertex_id) axis_b_fixed = vertex.is_fixed;
  }
  return expect(axis_a_fixed && axis_b_fixed,
                "ellipse: axis points flagged fixed");
}

bool test_ellipse_full_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_ellipse(10.0, 10.0, 20.0, 10.0, 10.0, 16.0);
  const auto params = sketch_params(document);
  const std::string ellipse_id = params.ellipses[0].id;

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {ellipse_id}, .kind = "ellipse"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("ellipse profile: " + reason).c_str())) {
    return false;
  }

  // The region carries the exact ellipse boundary edge.
  const auto& profile = params.profiles[0];
  const bool has_ellipse_edge = std::any_of(
      profile.boundary_edges.begin(), profile.boundary_edges.end(),
      [](const auto& be) { return be.entity_kind == "ellipse"; });
  return expect(has_ellipse_edge,
                "ellipse: profile carries an exact ellipse boundary edge");
}

bool test_ellipse_extrude_smoke() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_ellipse(10.0, 10.0, 20.0, 10.0, 10.0, 16.0);
  const auto params = sketch_params(document);
  const std::string profile_id = params.profiles[0].id;

  document = manager.extrude_profile(profile_id, 5.0, "new_body");

  // The extrude feature exists and compiles to one body.
  bool has_extrude = false;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "extrude") has_extrude = true;
  }
  const auto compiled = polysmith::core::compile_bodies(document);
  return expect(has_extrude && compiled.bodies.size() == 1,
                "ellipse: extrude produces a body from the ellipse profile");
}

bool test_ellipse_move_preserves_shape() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_ellipse(10.0, 10.0, 20.0, 10.0, 10.0, 16.0);
  const auto before = sketch_params(document);
  const std::string ellipse_id = before.ellipses[0].id;

  document = manager.move_sketch_entities({ellipse_id}, 5.0, 5.0, 0.0, 0.0,
                                          0.0);
  const auto after = sketch_params(document);
  const SketchEllipse& e = after.ellipses[0];
  if (!expect(near(e.center_x, 15.0) && near(e.center_y, 15.0),
              "ellipse: center follows the move")) {
    return false;
  }
  if (!expect(near(e.a, 10.0) && near(e.b, 6.0) && near(e.rotation, 0.0),
              "ellipse: a/b/rotation preserved by the move")) {
    return false;
  }
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {ellipse_id}, .kind = "ellipse"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("ellipse moved profile: " + reason).c_str());
}

bool test_construction_ellipse_excluded() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_ellipse(10.0, 10.0, 20.0, 10.0, 10.0, 16.0);
  // Construction ellipse must not add a profile.
  document = manager.add_sketch_ellipse(40.0, 40.0, 50.0, 40.0, 40.0, 46.0,
                                        /*is_construction=*/true);
  const auto params = sketch_params(document);
  if (!expect(params.ellipses.size() == 2,
              "ellipse: construction entity recorded")) {
    return false;
  }
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = {params.ellipses[0].id}, .kind = "ellipse"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("construction ellipse excluded: " + reason).c_str());
}

bool test_ellipse_trim_to_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Full ellipse a=50 b=20 at the origin, cut by a vertical line at
  // x=-20 whose endpoints land EXACTLY on the ellipse (the walk welds
  // endpoints to curves only via touch records — a line overhanging
  // the ellipse would be dropped as dangling). Trimming the left cap
  // converts the ellipse to ONE partial ellipse whose endpoints sit
  // exactly at the two intersections with the line.
  DocumentState document =
      manager.add_sketch_ellipse(0.0, 0.0, 50.0, 0.0, 0.0, 20.0);
  const double cap_y = 20.0 * std::sqrt(1.0 - (20.0 * 20.0) / (50.0 * 50.0));
  document = manager.add_sketch_line(-20.0, -cap_y, -20.0, cap_y);

  const auto before = sketch_params(document);
  const std::string ellipse_id = before.ellipses[0].id;
  document = manager.trim_sketch_entity(ellipse_id, -45.0, 0.0);

  const auto params = sketch_params(document);
  if (!expect(params.ellipses.size() == 1,
              "ellipse trim: one partial ellipse remains")) {
    return false;
  }
  const auto& e = params.ellipses.front();
  if (!expect(e.has_sweep, "ellipse trim: result carries a sweep")) {
    return false;
  }
  // The left cap (-x side) was deleted: the kept arc spans the +x
  // side, so its midpoint (a, 0) is material and (-a, 0) is not.
  const auto angle_at = [&](double px, double py) {
    const double cu = std::cos(e.rotation), su = std::sin(e.rotation);
    const double lx = (px - e.center_x) * cu + (py - e.center_y) * su;
    const double ly = -(px - e.center_x) * su + (py - e.center_y) * cu;
    return std::atan2(ly / e.b, lx / e.a);
  };
  const double mid = angle_at(50.0, 0.0);
  const double left = angle_at(-50.0, 0.0);
  const double s = e.sweep_start_angle;
  const double ee = e.sweep_end_angle;
  auto in_sweep = [&](double a) {
    double a2 = a < 0.0 ? a + 2.0 * kPi : a;
    double e2 = ee <= s ? ee + 2.0 * kPi : ee;
    double s2 = s < 0.0 ? s + 2.0 * kPi : s;
    if (a2 < s2) a2 += 2.0 * kPi;
    return a2 >= s2 - 1e-9 && a2 <= e2 + 1e-9;
  };
  if (!expect(in_sweep(mid) && !in_sweep(left),
              "ellipse trim: right cap kept, left cap deleted")) {
    return false;
  }

  // The complete region set: the lens between the elliptical arc and
  // the cutting line is one polygon profile.
  std::string reason;
  const std::vector<polysmith::test::ExpectedProfile> expected = {
      {{"ellipse-1", "line-1"}, "polygon"},
  };
  if (!profiles_match(document, expected, &reason)) {
    std::cerr << "  ellipse trim profiles: " << reason << "\n";
    for (const auto& p : params.profiles) {
      std::cerr << "    kind=" << p.kind << " ids=";
      for (const auto& id : p.line_ids) std::cerr << id << " ";
      std::cerr << "\n";
    }
    return false;
  }
  return true;
}

}  // namespace

int main() {
  if (!test_ellipse_creation()) return 1;
  if (!test_ellipse_full_profile()) return 1;
  if (!test_ellipse_extrude_smoke()) return 1;
  if (!test_ellipse_move_preserves_shape()) return 1;
  if (!test_construction_ellipse_excluded()) return 1;
  if (!test_ellipse_trim_to_arc()) return 1;

  std::cout << "ellipse_test passed\n";
  return 0;
}
