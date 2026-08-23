// Regression tests for the sketch extend tool (feature/sketch, SK4).
//
// extend_sketch_entity extends a line (along its infinite support)
// or an arc (along its full circle) from the end nearest the click
// to the nearest intersection with another non-construction entity.
// Non-parametric, like trim: the endpoint moves and the recompute
// re-derives everything else. Unlike trim, extend preserves the
// line's H/V constraint and dimensions; arc angle dimensions flip
// to driven.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/sketch/sketch_geometry_types.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchArc;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
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

const SketchLine* find_line(const SketchFeatureParameters& params,
                            const std::string& id) {
  const auto it = std::find_if(params.lines.begin(), params.lines.end(),
                               [&](const auto& l) { return l.id == id; });
  return it == params.lines.end() ? nullptr : &*it;
}

const SketchArc* find_arc(const SketchFeatureParameters& params,
                          const std::string& id) {
  const auto it = std::find_if(params.arcs.begin(), params.arcs.end(),
                               [&](const auto& a) { return a.id == id; });
  return it == params.arcs.end() ? nullptr : &*it;
}

// Line (0,0)-(5,0) between two verticals: extending the end hits the
// right vertical at (10,0); extending the start hits the left
// vertical at (-5,0).
bool test_line_extend_both_directions() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 5.0, 0.0);
  manager.add_sketch_line(10.0, 0.0, 10.0, 10.0);
  manager.add_sketch_line(-5.0, 0.0, -5.0, 10.0);
  const std::string line_id = sketch_params(document).lines[0].id;

  // Extend the end (click beyond (5,0) toward the right vertical).
  document = manager.extend_sketch_entity(line_id, 8.0, 0.0);
  const auto after_end_snapshot = sketch_params(document);
  const auto* after_end = find_line(after_end_snapshot, line_id);
  if (!expect(after_end != nullptr &&
                  near(after_end->end_x, 10.0) && near(after_end->end_y, 0.0) &&
                  near(after_end->start_x, 0.0) && near(after_end->start_y, 0.0),
              "extend: line end reaches the right vertical")) {
    return false;
  }

  // Extend the start (click beyond (0,0) toward the left vertical).
  document = manager.extend_sketch_entity(line_id, -3.0, 0.0);
  const auto after_start_snapshot = sketch_params(document);
  const auto* after_start = find_line(after_start_snapshot, line_id);
  return expect(after_start != nullptr &&
                    near(after_start->start_x, -5.0) &&
                    near(after_start->start_y, 0.0) &&
                    near(after_start->end_x, 10.0) &&
                    near(after_start->end_y, 0.0),
                "extend: line start reaches the left vertical");
}

// Three full rectangle sides plus a partial left side: extending the
// partial closes the loop, and the closed rectangle must be detected
// as a single polygon profile with all four line ids.
bool test_extend_closes_rectangle_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  manager.add_sketch_line(10.0, 0.0, 10.0, 10.0);
  manager.add_sketch_line(10.0, 10.0, 0.0, 10.0);
  document = manager.add_sketch_line(0.0, 10.0, 0.0, 4.0);
  const auto before = sketch_params(document);
  if (!expect(before.lines.size() == 4,
              "extend profile: 4 lines before the extend")) {
    return false;
  }
  // The last line is the partial left side.
  const std::string partial_id = before.lines[3].id;

  // Extend the partial's end (click below it) down to the bottom
  // line's start at (0,0).
  document = manager.extend_sketch_entity(partial_id, 0.0, 2.0);
  const auto after = sketch_params(document);
  const auto* partial = find_line(after, partial_id);
  if (!expect(partial != nullptr && near(partial->end_x, 0.0) &&
                  near(partial->end_y, 0.0),
              "extend profile: partial side reaches the bottom corner")) {
    return false;
  }

  std::vector<std::string> expected_ids;
  for (const auto& line : after.lines) expected_ids.push_back(line.id);
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("extend profile: closed rectangle — " + reason).c_str());
}

// Line (2,8)-(4,8) extends along +x onto the quarter arc's circle at
// (6,8) — the intersection must lie inside the arc's sweep.
bool test_line_extend_to_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(2.0, 8.0, 4.0, 8.0);
  manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  const std::string line_id = sketch_params(document).lines[0].id;

  document = manager.extend_sketch_entity(line_id, 6.0, 8.0);
  const auto line_snapshot = sketch_params(document);
  const auto* line = find_line(line_snapshot, line_id);
  return expect(line != nullptr && near(line->end_x, 6.0) &&
                    near(line->end_y, 8.0) &&
                    near(std::hypot(line->end_x, line->end_y), 10.0),
                "extend: line end lands on the arc's circle");
}

// Quarter arc (10,0)->(0,10) ccw around (0,0) extends its end along
// the full circle to the vertical line at (0,-10) — the sweep grows,
// the radius is preserved.
bool test_arc_extend_to_line() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  manager.add_sketch_line(0.0, -20.0, 0.0, -5.0);
  const std::string arc_id = sketch_params(document).arcs[0].id;

  document = manager.extend_sketch_entity(arc_id, 1.0, 12.0);
  const auto arc_snapshot = sketch_params(document);
  const auto* arc = find_arc(arc_snapshot, arc_id);
  return expect(arc != nullptr && near(arc->end_x, 0.0) &&
                    near(arc->end_y, -10.0) && near(arc->radius, 10.0) &&
                    arc->ccw,
                "extend: arc end reaches the vertical line, radius preserved");
}

bool test_no_intersection_error() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 5.0, 0.0);
  const std::string line_id = sketch_params(document).lines[0].id;

  bool threw = false;
  try {
    (void)manager.extend_sketch_entity(line_id, 8.0, 0.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "extend: no intersection throws");
}

// Generated slot outlines are derived geometry — extending them must
// be rejected (the expansion pass would regenerate them anyway).
bool test_generated_rejection() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(0.0, 0.0, 10.0, 2.0, 0.0, false);
  const auto before = sketch_params(document);
  const auto generated = std::find_if(
      before.lines.begin(), before.lines.end(),
      [&](const auto& l) { return l.generated_by.has_value(); });
  if (!expect(generated != before.lines.end(),
              "extend: slot expansion produced generated lines")) {
    return false;
  }

  bool threw = false;
  try {
    (void)manager.extend_sketch_entity(generated->id, 0.0, 0.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "extend: generated entities are rejected");
}

// Extend preserves the line's inferred H constraint, and an arc
// angle dimension flips to driven when the extension changes the
// sweep.
bool test_extend_preserves_constraint_and_flips_angle_dim() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Horizontal line between two verticals.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 5.0, 0.0);
  manager.add_sketch_line(10.0, -5.0, 10.0, 5.0);
  const std::string line_id = sketch_params(document).lines[0].id;

  document = manager.extend_sketch_entity(line_id, 8.0, 0.0);
  const auto hv_snapshot = sketch_params(document);
  const auto* line = find_line(hv_snapshot, line_id);
  if (!expect(line != nullptr && line->constraint.has_value() &&
                  line->constraint.value() == "horizontal",
              "extend: H constraint survives the extension")) {
    return false;
  }

  // Quarter arc with a driving angle dimension, extended to a
  // vertical line — the dimension flips to driven.
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0,
                                    "center_start_end");
  manager.add_sketch_line(0.0, -20.0, 0.0, -5.0);
  const std::string arc_id = sketch_params(document).arcs[0].id;
  document = manager.add_sketch_arc_angle_dimension(arc_id);

  document = manager.extend_sketch_entity(arc_id, 1.0, 12.0);
  const auto after = sketch_params(document);
  const auto* arc = find_arc(after, arc_id);
  if (!expect(arc != nullptr && near(arc->end_y, -10.0),
              "extend: arc with angle dim extends")) {
    return false;
  }
  const auto dim_it = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& d) { return d.kind == "arc_angle" && d.entity_id == arc_id; });
  return expect(dim_it != after.dimensions.end() && dim_it->driven,
                "extend: arc angle dimension flips to driven");
}

}  // namespace

int main() {
  if (!test_line_extend_both_directions()) return 1;
  if (!test_extend_closes_rectangle_profile()) return 1;
  if (!test_line_extend_to_arc()) return 1;
  if (!test_arc_extend_to_line()) return 1;
  if (!test_no_intersection_error()) return 1;
  if (!test_generated_rejection()) return 1;
  if (!test_extend_preserves_constraint_and_flips_angle_dim()) return 1;

  std::cout << "extend_test passed\n";
  return 0;
}
