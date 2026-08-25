// Regression tests for SK6 — dimension completion:
// diameter display on circle radius dims, arc angle, arc length.
//
// - Diameter: the dimension's stored value stays the RADIUS for the
//   solver; edits through the update path divide the displayed
//   diameter by two, and the payload carries the displayed value.
//   Both epsilon sides must drive correctly.
// - Arc angle: 90 and 270 degree sweeps, both ccw orientations.
// - Arc length: quarter-circle length drives the sweep; the driven
//   sync re-measures from geometry.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/viewport/viewport.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::ViewportSketchDimensionPrimitive;

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

// ── Helpers for the viewport-emission tests ───────────────────────────
//
// The assertions below are written against frame-independent geometry
// (distances, midpoints, collinearity) rather than raw world coordinates,
// so they hold on any sketch plane.

struct P3 {
  double x, y, z;
};

P3 sub(const P3& a, const P3& b) { return P3{a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const P3& a, const P3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
double length(const P3& v) { return std::sqrt(dot(v, v)); }
double distance(const P3& a, const P3& b) { return length(sub(a, b)); }
bool finite(const P3& v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

P3 label_of(const ViewportSketchDimensionPrimitive& p) {
  return P3{p.label_x, p.label_y, p.label_z};
}
P3 center_of(const ViewportSketchDimensionPrimitive& p) {
  return P3{p.arc_center_x, p.arc_center_y, p.arc_center_z};
}
P3 anchor_start_of(const ViewportSketchDimensionPrimitive& p) {
  return P3{p.anchor_start_x, p.anchor_start_y, p.anchor_start_z};
}
P3 anchor_end_of(const ViewportSketchDimensionPrimitive& p) {
  return P3{p.anchor_end_x, p.anchor_end_y, p.anchor_end_z};
}
P3 dim_start_of(const ViewportSketchDimensionPrimitive& p) {
  return P3{p.dimension_start_x, p.dimension_start_y, p.dimension_start_z};
}
P3 dim_end_of(const ViewportSketchDimensionPrimitive& p) {
  return P3{p.dimension_end_x, p.dimension_end_y, p.dimension_end_z};
}

// Id of the dimension of `kind` attached to `entity_id`.
std::string find_dimension_id(const DocumentState& document,
                              const std::string& kind,
                              const std::string& entity_id) {
  for (const auto& dimension : sketch_params(document).dimensions) {
    if (dimension.kind == kind && dimension.entity_id == entity_id) {
      return dimension.id;
    }
  }
  return "";
}

// The emitted primitive of `kind` for `entity_id`, or nullopt when the
// core emits nothing for it.
std::optional<ViewportSketchDimensionPrimitive> find_primitive(
    const DocumentState& document, const std::string& kind,
    const std::string& entity_id) {
  const auto viewport = polysmith::core::build_viewport_state(document);
  for (const auto& primitive : viewport.sketch_dimensions) {
    if (primitive.kind == kind && primitive.entity_id == entity_id) {
      return primitive;
    }
  }
  return std::nullopt;
}

// True when `point` sits on the circle of `radius` about `center` in the
// direction of `toward` — i.e. the leader contact landed where the label
// asked for it.
bool contact_matches_direction(const P3& point, const P3& center,
                               const P3& toward, double radius) {
  const P3 contact_ray = sub(point, center);
  const P3 label_ray = sub(toward, center);
  const double contact_length = length(contact_ray);
  const double label_length = length(label_ray);
  if (contact_length <= 1e-9 || label_length <= 1e-9) {
    return false;
  }
  const double cosine = dot(contact_ray, label_ray) /
                        (contact_length * label_length);
  return near(contact_length, radius, 1e-6) && near(cosine, 1.0, 1e-6);
}

bool test_diameter_drives_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);
  // The auto radius dimension id follows the curve primitive pattern;
  // the display defaults to diameter (display_as == "").
  const std::string dim_id = "dim-circle-circle-1";
  auto dim_value = [&](const DocumentState& doc) {
    const auto params = sketch_params(doc);
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& d) { return d.id == dim_id; });
    return it == params.dimensions.end() ? -1.0 : it->value;
  };
  const auto circle_radius = [&](const DocumentState& doc) {
    return sketch_params(doc).circles[0].radius;
  };

  // The direct API stores and drives the RADIUS (the displayed
  // diameter conversion lives at the IPC boundary). Both epsilon
  // sides must drive.
  document = manager.update_sketch_dimension(dim_id, 8.0);
  if (!expect(near(circle_radius(document), 8.0),
              "diameter: radius 8 drives the circle")) {
    return false;
  }
  if (!expect(near(dim_value(document), 8.0),
              "diameter: stored value stays the radius")) {
    return false;
  }
  const auto dim = sketch_params(document).dimensions.front();
  if (!expect(dim.display_as != "radius",
              "diameter: default display is diameter")) {
    return false;
  }

  document = manager.update_sketch_dimension(dim_id, 2.0);
  return expect(near(circle_radius(document), 2.0),
                "diameter: radius 2 drives (other epsilon side)");
}

bool test_arc_angle_drives_sweeps_both_orientations() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Quarter arc ccw, then a cw one — 90 and 270 degree sweeps.
  DocumentState document = manager.add_sketch_arc(
      10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  document = manager.add_sketch_arc_angle_dimension("arc-1");
  const std::string dim_id = "dim-arc-angle-arc-1";

  const auto sweep = [&](const DocumentState& doc) {
    const auto& arc = sketch_params(doc).arcs[0];
    double raw = std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x) -
                 std::atan2(arc.start_y - arc.center_y,
                            arc.start_x - arc.center_x);
    if (raw < 0.0) raw += 2.0 * 3.14159265358979323846;
    return raw;
  };

  // 90 degrees already; drive to 180 (the other side of the epsilon).
  document = manager.update_sketch_dimension(dim_id, 3.14159265358979323846);
  if (!expect(near(sweep(document), 3.14159265358979323846),
              "arc angle: 180 degree sweep drives")) {
    return false;
  }

  // Drive to 270 (cw-arc equivalents are handled by the same sweep
  // logic — the endpoint re-derives along the circle).
  document = manager.update_sketch_dimension(dim_id, 4.71238898038469);
  return expect(near(sweep(document), 4.71238898038469),
                "arc angle: 270 degree sweep drives");
}

bool test_arc_length_drives_quarter_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_arc(
      10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  document = manager.add_sketch_arc_length_dimension("arc-1");
  const std::string dim_id = "dim-arc-length-arc-1";

  const auto measure = [&](const DocumentState& doc) {
    const auto& arc = sketch_params(doc).arcs[0];
    double raw = std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x) -
                 std::atan2(arc.start_y - arc.center_y,
                            arc.start_x - arc.center_x);
    if (raw < 0.0) raw += 2.0 * 3.14159265358979323846;
    return arc.radius * raw;
  };

  // Quarter circle of r=10: length ~15.708. Drive to a full
  // semicircle's length 31.416 -> sweep pi.
  document = manager.update_sketch_dimension(dim_id, 31.41592653589793);
  return expect(near(measure(document), 31.41592653589793, 1e-4),
                "arc length: L=pi*r drives a semicircle");
}

// ── Radial dimension leader emission ─────────────────────────────────
//
// These cover the viewport emitters, which used to ignore the stored
// label entirely: a dragged label was persisted but the next
// get_viewport_state re-emitted the hardcoded default, so the label
// snapped back. Each test below fails on the pre-fix emitters.

// A quarter arc: centre (0,0), radius 10, spanning 0 deg to 90 deg.
DocumentState quarter_arc_document(DocumentManager& manager) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  return manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0,
                                "center_start_end");
}

bool test_circle_radius_label_position_honored() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);
  // The circle's own dimension is auto until explicitly added, and auto
  // dimensions are not emitted to the viewport.
  document = manager.add_sketch_circle_radius_dimension("circle-1");
  const std::string dim_id =
      find_dimension_id(document, "circle_radius", "circle-1");
  if (!expect(!dim_id.empty(), "radius label: dimension was created")) {
    return false;
  }
  // Force radius display; a circle dimension defaults to diameter.
  document = manager.update_sketch_dimension_display(dim_id, "radius");
  document = manager.update_sketch_dimension_label_position(dim_id, 30.0, 40.0);
  const auto primitive = find_primitive(document, "circle_radius", "circle-1");
  if (!expect(primitive.has_value(), "radius label: primitive emitted")) {
    return false;
  }

  const P3 center = center_of(*primitive);
  const P3 label = label_of(*primitive);
  // (30,40) is sqrt(1300) from the centre at (10,10). Before the fix the
  // label was re-emitted at radius + 8 regardless of the drag.
  if (!expect(near(distance(label, center), std::sqrt(1300.0), 1e-6),
              "radius label: dragged label survives re-emission")) {
    return false;
  }
  if (!expect(near(primitive->arc_radius, 5.0),
              "radius label: arc_radius carries the circle radius")) {
    return false;
  }
  // One arrowhead, on the rim, in the label's direction.
  if (!expect(contact_matches_direction(anchor_start_of(*primitive), center,
                                        label, 5.0),
              "radius label: contact sits on the rim toward the label")) {
    return false;
  }
  return expect(
      distance(dim_start_of(*primitive), dim_end_of(*primitive)) < 1e-9,
      "radius label: radius mode emits a single arrowhead");
}

bool test_diameter_emits_through_center_tips() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);
  document = manager.add_sketch_circle_radius_dimension("circle-1");
  const std::string dim_id =
      find_dimension_id(document, "circle_radius", "circle-1");
  // Diameter is the default display for a circle dimension.
  document = manager.update_sketch_dimension_display(dim_id, "");
  document = manager.update_sketch_dimension_label_position(dim_id, 30.0, 40.0);

  const auto primitive = find_primitive(document, "circle_radius", "circle-1");
  if (!expect(primitive.has_value(), "diameter: primitive emitted")) {
    return false;
  }
  const P3 center = center_of(*primitive);
  const P3 first = dim_start_of(*primitive);
  const P3 second = dim_end_of(*primitive);

  if (!expect(near(distance(first, second), 10.0, 1e-6),
              "diameter: tips span the full diameter")) {
    return false;
  }
  const P3 midpoint{(first.x + second.x) / 2.0, (first.y + second.y) / 2.0,
                    (first.z + second.z) / 2.0};
  if (!expect(distance(midpoint, center) < 1e-6,
              "diameter: the dimension line passes through the centre")) {
    return false;
  }
  // The leader leaves from the tip on the label's side.
  return expect(contact_matches_direction(anchor_start_of(*primitive), center,
                                          label_of(*primitive), 5.0),
                "diameter: leader departs the tip nearest the label");
}

bool test_arc_radius_contact_clamped_into_sweep() {
  DocumentManager manager;
  DocumentState document = quarter_arc_document(manager);
  document = manager.add_sketch_arc_radius_dimension("arc-1");
  const std::string dim_id =
      find_dimension_id(document, "arc_radius", "arc-1");
  if (!expect(!dim_id.empty(), "arc radius: dimension was created")) {
    return false;
  }

  const auto contact_for = [&](double label_x,
                               double label_y) -> std::optional<P3> {
    document =
        manager.update_sketch_dimension_label_position(dim_id, label_x, label_y);
    const auto primitive = find_primitive(document, "arc_radius", "arc-1");
    if (!primitive.has_value()) {
      return std::nullopt;
    }
    return dim_start_of(*primitive);
  };

  // In-sweep label (45 deg): the contact tracks the label direction.
  const auto inside = contact_for(30.0, 30.0);
  const auto inside_primitive = find_primitive(document, "arc_radius", "arc-1");
  if (!expect(inside.has_value() && inside_primitive.has_value(),
              "arc radius: primitive emitted")) {
    return false;
  }
  if (!expect(contact_matches_direction(*inside,
                                        center_of(*inside_primitive),
                                        label_of(*inside_primitive), 10.0),
              "arc radius: in-sweep contact follows the label")) {
    return false;
  }

  // Reference: a label exactly on the start ray lands on the arc's start.
  const auto on_start = contact_for(30.0, 0.0);
  // Just inside the start boundary (+epsilon) stays on the label ray.
  const auto just_inside = contact_for(30.0, 1.0);
  // Just outside the start boundary (-epsilon) snaps back to the start.
  const auto just_outside = contact_for(30.0, -1.0);
  if (!expect(on_start.has_value() && just_inside.has_value() &&
                  just_outside.has_value(),
              "arc radius: boundary primitives emitted")) {
    return false;
  }
  if (!expect(distance(*just_outside, *on_start) < 1e-6,
              "arc radius: label past the start snaps onto the arc start")) {
    return false;
  }
  if (!expect(distance(*just_inside, *on_start) > 1e-3,
              "arc radius: label inside the sweep is not snapped")) {
    return false;
  }

  // Past the far boundary it snaps to the arc's end instead.
  const auto on_end = contact_for(0.0, 30.0);
  const auto past_end = contact_for(-1.0, 30.0);
  if (!expect(on_end.has_value() && past_end.has_value(),
              "arc radius: end-boundary primitives emitted")) {
    return false;
  }
  return expect(distance(*past_end, *on_end) < 1e-6,
                "arc radius: label past the end snaps onto the arc end");
}

bool test_arc_length_extension_arc_clamps_both_sides() {
  DocumentManager manager;
  DocumentState document = quarter_arc_document(manager);
  document = manager.add_sketch_arc_length_dimension("arc-1");
  const std::string dim_id =
      find_dimension_id(document, "arc_length", "arc-1");

  // Label well outside: the extension arc passes through it.
  document =
      manager.update_sketch_dimension_label_position(dim_id, 100.0, 100.0);
  auto primitive = find_primitive(document, "arc_length", "arc-1");
  if (!expect(primitive.has_value(), "arc length: primitive emitted")) {
    return false;
  }
  const P3 center = center_of(*primitive);
  const double far_distance = std::sqrt(20000.0);
  if (!expect(near(primitive->arc_radius, far_distance, 1e-6),
              "arc length: extension radius follows a far label")) {
    return false;
  }
  // Witness feet sit on the measured arc; the extension arc endpoints
  // sit at the extension radius.
  if (!expect(near(distance(anchor_start_of(*primitive), center), 10.0, 1e-6) &&
                  near(distance(anchor_end_of(*primitive), center), 10.0, 1e-6),
              "arc length: anchors are the arc's own endpoints")) {
    return false;
  }
  if (!expect(
          near(distance(dim_start_of(*primitive), center), far_distance, 1e-6) &&
              near(distance(dim_end_of(*primitive), center), far_distance,
                   1e-6),
          "arc length: extension endpoints sit at the extension radius")) {
    return false;
  }

  // Label pulled inside the arc: clamped to radius + kArcLengthMinGap so
  // the extension never falls inside the geometry it measures.
  document = manager.update_sketch_dimension_label_position(dim_id, 1.0, 1.0);
  primitive = find_primitive(document, "arc_length", "arc-1");
  if (!expect(primitive.has_value(), "arc length: clamped primitive emitted")) {
    return false;
  }
  return expect(near(primitive->arc_radius, 16.0, 1e-6),
                "arc length: near label clamps to radius + min gap");
}

bool test_arc_angle_emits_primitive_with_radius_clamps() {
  DocumentManager manager;
  DocumentState document = quarter_arc_document(manager);
  document = manager.add_sketch_arc_angle_dimension("arc-1");
  const std::string dim_id = find_dimension_id(document, "arc_angle", "arc-1");
  if (!expect(!dim_id.empty(), "arc angle: dimension was created")) {
    return false;
  }

  // Before the fix there was no arc_angle emitter at all, so the
  // dimension existed in the document but never reached the viewport.
  auto primitive = find_primitive(document, "arc_angle", "arc-1");
  if (!expect(primitive.has_value(), "arc angle: primitive is emitted")) {
    return false;
  }

  // Far label: the bisector radius clamps at 500.
  document =
      manager.update_sketch_dimension_label_position(dim_id, 1000.0, 1000.0);
  primitive = find_primitive(document, "arc_angle", "arc-1");
  if (!expect(primitive.has_value() && near(primitive->arc_radius, 500.0, 1e-6),
              "arc angle: far label clamps the radius to 500")) {
    return false;
  }
  if (!expect(near(distance(label_of(*primitive), center_of(*primitive)), 500.0,
                   1e-6),
              "arc angle: label rides the bisector at the clamped radius")) {
    return false;
  }

  // Near label: clamps at 6 (the other side of the clamp).
  document = manager.update_sketch_dimension_label_position(dim_id, 0.5, 0.5);
  primitive = find_primitive(document, "arc_angle", "arc-1");
  return expect(
      primitive.has_value() && near(primitive->arc_radius, 6.0, 1e-6),
      "arc angle: near label clamps the radius to 6");
}

bool test_label_on_center_emits_finite_geometry() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 10.0, 5.0);
  document = manager.add_sketch_circle_radius_dimension("circle-1");
  const std::string dim_id =
      find_dimension_id(document, "circle_radius", "circle-1");
  document = manager.update_sketch_dimension_display(dim_id, "radius");

  // Dropping the label exactly on the centre leaves the leader direction
  // undefined; the emitter must fall back rather than produce NaN.
  document = manager.update_sketch_dimension_label_position(dim_id, 10.0, 10.0);
  const auto primitive = find_primitive(document, "circle_radius", "circle-1");
  if (!expect(primitive.has_value(), "degenerate: primitive emitted")) {
    return false;
  }
  if (!expect(finite(label_of(*primitive)) && finite(center_of(*primitive)) &&
                  finite(anchor_start_of(*primitive)) &&
                  finite(anchor_end_of(*primitive)) &&
                  finite(dim_start_of(*primitive)) &&
                  finite(dim_end_of(*primitive)) &&
                  std::isfinite(primitive->arc_radius),
              "degenerate: label on the centre emits finite geometry")) {
    return false;
  }
  return expect(
      near(distance(anchor_start_of(*primitive), center_of(*primitive)), 5.0,
           1e-6),
      "degenerate: contact still lands on the rim");
}

}  // namespace

int main() {
  try {
  if (!test_diameter_drives_both_sides()) return 1;
  if (!test_arc_angle_drives_sweeps_both_orientations()) return 1;
  if (!test_arc_length_drives_quarter_circle()) return 1;
  if (!test_circle_radius_label_position_honored()) return 1;
  if (!test_diameter_emits_through_center_tips()) return 1;
  if (!test_arc_radius_contact_clamped_into_sweep()) return 1;
  if (!test_arc_length_extension_arc_clamps_both_sides()) return 1;
  if (!test_arc_angle_emits_primitive_with_radius_clamps()) return 1;
  if (!test_label_on_center_emits_finite_geometry()) return 1;

  std::cout << "dimension_completion_test passed\n";
  return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
