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

bool test_ellipse_trim_rejected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_ellipse(10.0, 10.0, 20.0, 10.0, 10.0, 16.0);
  const auto params = sketch_params(document);
  const std::string ellipse_id = params.ellipses[0].id;

  // The trim engine handles line/circle/arc only — an ellipse id must
  // be rejected, not silently ignored or crashed on.
  bool threw = false;
  try {
    (void)manager.trim_sketch_entity(ellipse_id, 10.0, 16.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "ellipse: trim on an ellipse is rejected");
}

}  // namespace

int main() {
  if (!test_ellipse_creation()) return 1;
  if (!test_ellipse_full_profile()) return 1;
  if (!test_ellipse_extrude_smoke()) return 1;
  if (!test_ellipse_move_preserves_shape()) return 1;
  if (!test_construction_ellipse_excluded()) return 1;
  if (!test_ellipse_trim_rejected()) return 1;

  std::cout << "ellipse_test passed\n";
  return 0;
}
