// Regression tests for the sketch Move tool's core command
// (move_sketch_entities): rigid translate + rotate around a center,
// fixed vertices stay put, connected geometry ripples, rotation frees
// H/V constraints, circle/arc radii are preserved, exactly one undo
// step, and dimensions re-sync.
//
// The profile-set assertions use profiles_match from sketch_test_utils.h
// (complete region-set matching, not presence-only).

#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchVertex;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

// Returns a VALUE COPY of the active sketch's parameters — callers
// reassign `document` after setup, so a reference would dangle.
SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

const SketchVertex* find_vertex(const SketchFeatureParameters& params,
                                const std::string& id) {
  for (const auto& vertex : params.vertices) {
    if (vertex.id == id) return &vertex;
  }
  return nullptr;
}

std::string line_id_at(const SketchFeatureParameters& params, int index) {
  return params.lines.at(index).id;
}

bool points_near(double x1, double y1, double x2, double y2,
                 double tolerance = 1.0e-6) {
  return std::abs(x1 - x2) < tolerance && std::abs(y1 - y2) < tolerance;
}

// Moves the given entities; returns the new document state.
DocumentState move(DocumentManager& manager,
                   const std::vector<std::string>& entity_ids,
                   double dx, double dy, double center_x, double center_y,
                   double angle_deg) {
  return manager.move_sketch_entities(entity_ids, dx, dy, center_x, center_y,
                                      angle_deg);
}

bool test_translate_rectangle_rigid() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 100.0, 80.0);
  const auto before = sketch_params(document);

  // Pin the sides to explicit H/V constraints matching their actual
  // orientation, so the translate test covers constraint preservation
  // deterministically (the rectangle sides are already axis-aligned).
  std::vector<std::string> line_ids;
  for (const auto& line : before.lines) {
    line_ids.push_back(line.id);
    const bool horizontal =
        std::abs(line.start_y - line.end_y) < 1.0e-6;
    document = manager.set_sketch_line_constraint(
        line.id, std::string(horizontal ? "horizontal" : "vertical"));
  }

  document = move(manager, line_ids, /*dx=*/10.0, /*dy=*/5.0, /*cx=*/0.0,
                  /*cy=*/0.0, /*angle=*/0.0);

  const auto after = sketch_params(document);
  for (const auto& vertex : after.vertices) {
    const SketchVertex* previous = find_vertex(before, vertex.id);
    if (!expect(previous != nullptr,
                "translate: every vertex must survive the move")) {
      return false;
    }
    if (!expect(points_near(vertex.x, vertex.y, previous->x + 10.0,
                            previous->y + 5.0),
                "translate: vertex moved by exactly (10,5)")) {
      return false;
    }
  }

  for (const auto& line : after.lines) {
    if (!expect(line.constraint.has_value(),
                "translate: H/V constraints survive a pure translation")) {
      return false;
    }
  }

  std::string reason;
  if (!expect(polysmith::test::profiles_match(
                  document,
                  {{
                      .entity_ids = line_ids,
                      .kind = "polygon",
                      .has_source_circle_id = false,
                  }},
                  &reason),
              ("translate: profile set must stay the single rectangle: " +
               reason).c_str())) {
    return false;
  }

  return true;
}

bool test_move_keeps_fixed_vertex() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 100.0, 80.0);
  const auto before = sketch_params(document);

  std::string corner_bl;
  for (const auto& vertex : before.vertices) {
    if (points_near(vertex.x, vertex.y, 0.0, 0.0)) {
      corner_bl = vertex.id;
    }
  }
  if (!expect(!corner_bl.empty(), "fixed: failed to locate bottom-left corner")) {
    return false;
  }
  document = manager.set_sketch_vertex_fixed(corner_bl, true);

  std::vector<std::string> line_ids;
  for (const auto& line : before.lines) {
    line_ids.push_back(line.id);
  }
  document = move(manager, line_ids, 10.0, 5.0, 0.0, 0.0, 0.0);

  const auto after = sketch_params(document);
  const SketchVertex* fixed = find_vertex(after, corner_bl);
  if (!expect(fixed != nullptr, "fixed: fixed vertex must survive")) {
    return false;
  }
  if (!expect(points_near(fixed->x, fixed->y, 0.0, 0.0),
              "fixed: fixed vertex must not move")) {
    return false;
  }

  bool some_vertex_moved = false;
  for (const auto& vertex : after.vertices) {
    const SketchVertex* previous = find_vertex(before, vertex.id);
    if (previous && !points_near(vertex.x, vertex.y, previous->x, previous->y)) {
      some_vertex_moved = true;
    }
  }
  return expect(some_vertex_moved,
                "fixed: at least one free vertex must move");
}

bool test_rotate_strips_hv_and_preserves_geometry() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // L-shape: horizontal (0,0)-(10,0), vertical (10,0)-(10,10).
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_line(10.0, 0.0, 10.0, 10.0);
  const auto before = sketch_params(document);
  const std::string line_a = before.lines[0].id;
  const std::string line_b = before.lines[1].id;
  document = manager.set_sketch_line_constraint(line_a, std::string("horizontal"));
  document = manager.set_sketch_line_constraint(line_b, std::string("vertical"));

  // Rotate 90° about the shared vertex (10,0).
  document = move(manager, {line_a, line_b}, 0.0, 0.0, /*cx=*/10.0,
                  /*cy=*/0.0, /*angle=*/90.0);

  const auto after = sketch_params(document);
  const auto& rotated_a = *std::find_if(
      after.lines.begin(), after.lines.end(),
      [&](const auto& line) { return line.id == line_a; });
  const auto& rotated_b = *std::find_if(
      after.lines.begin(), after.lines.end(),
      [&](const auto& line) { return line.id == line_b; });

  if (!expect(!rotated_a.constraint.has_value() &&
                  !rotated_b.constraint.has_value(),
              "rotate: H/V constraints are freed on rotated lines")) {
    return false;
  }

  // (0,0) -> (10,-10); (10,0) stays; (10,10) -> (0,0).
  auto line_connects = [&](const auto& line, double x1, double y1, double x2,
                           double y2) {
    return (points_near(line.start_x, line.start_y, x1, y1) &&
            points_near(line.end_x, line.end_y, x2, y2)) ||
           (points_near(line.start_x, line.start_y, x2, y2) &&
            points_near(line.end_x, line.end_y, x1, y1));
  };

  if (!expect(line_connects(rotated_a, 10.0, -10.0, 10.0, 0.0),
              "rotate: horizontal line rotated onto the vertical axis")) {
    return false;
  }
  if (!expect(line_connects(rotated_b, 10.0, 0.0, 0.0, 0.0),
              "rotate: vertical line rotated onto the horizontal axis")) {
    return false;
  }
  // Shared vertex must stay coincident (same vertex id, one position).
  if (!expect(rotated_a.start_vertex_id == rotated_b.start_vertex_id ||
                  rotated_a.start_vertex_id == rotated_b.end_vertex_id ||
                  rotated_a.end_vertex_id == rotated_b.start_vertex_id ||
                  rotated_a.end_vertex_id == rotated_b.end_vertex_id,
              "rotate: shared vertex id is preserved")) {
    return false;
  }
  return true;
}

bool test_translate_circle_and_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document =
      manager.add_sketch_circle(5.0, 5.0, 3.0, /*is_construction=*/false);
  document = manager.add_sketch_arc(8.0, 5.0, 5.0, 8.0,
                                    /*anchor_x=*/5.0, /*anchor_y=*/5.0,
                                    /*mode=*/"center_start_end",
                                    /*is_construction=*/false);

  const auto before = sketch_params(document);
  if (!expect(!before.circles.empty() && !before.arcs.empty(),
              "circle/arc: setup failed")) {
    return false;
  }
  const double before_arc_radius = before.arcs[0].radius;
  const bool before_arc_ccw = before.arcs[0].ccw;
  const std::string circle_id = before.circles[0].id;
  const std::string arc_id = before.arcs[0].id;

  document = move(manager, {circle_id, arc_id}, 10.0, 10.0, 0.0, 0.0, 0.0);

  const auto after = sketch_params(document);
  const auto& circle = after.circles[0];
  const auto& arc = after.arcs[0];

  if (!expect(points_near(circle.center_x, circle.center_y, 15.0, 15.0),
              "circle: center translated by (10,10)")) {
    return false;
  }
  if (!expect(std::abs(circle.radius - 3.0) < 1.0e-6,
              "circle: radius unchanged")) {
    return false;
  }
  if (!expect(points_near(arc.center_x, arc.center_y, 15.0, 15.0) &&
                  points_near(arc.start_x, arc.start_y, 18.0, 15.0) &&
                  points_near(arc.end_x, arc.end_y, 15.0, 18.0),
              "arc: center + endpoints translated by (10,10)")) {
    return false;
  }
  if (!expect(std::abs(arc.radius - before_arc_radius) < 1.0e-6 &&
                  arc.ccw == before_arc_ccw,
              "arc: radius and ccw unchanged")) {
    return false;
  }
  const double start_dist = std::hypot(arc.start_x - arc.center_x,
                                       arc.start_y - arc.center_y);
  const double end_dist = std::hypot(arc.end_x - arc.center_x,
                                     arc.end_y - arc.center_y);
  return expect(std::abs(start_dist - before_arc_radius) < 1.0e-6 &&
                    std::abs(end_dist - before_arc_radius) < 1.0e-6,
                "arc: endpoint-to-center distances unchanged");
}

bool test_single_undo_step() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_line(10.0, 0.0, 10.0, 10.0);
  const auto before = sketch_params(document);
  const std::string line_a = before.lines[0].id;
  const std::string line_b = before.lines[1].id;
  document = manager.set_sketch_line_constraint(line_a, std::string("horizontal"));
  document = manager.set_sketch_line_constraint(line_b, std::string("vertical"));

  document = move(manager, {line_a, line_b}, 0.0, 0.0, 10.0, 0.0, 90.0);

  // A single undo must restore BOTH the positions and the H/V
  // constraints that the rotation freed.
  document = manager.undo();

  const auto restored = sketch_params(document);
  const auto& restored_a = *std::find_if(
      restored.lines.begin(), restored.lines.end(),
      [&](const auto& line) { return line.id == line_a; });
  const auto& restored_b = *std::find_if(
      restored.lines.begin(), restored.lines.end(),
      [&](const auto& line) { return line.id == line_b; });

  if (!expect(restored_a.constraint.has_value() &&
                  restored_b.constraint.has_value(),
              "undo: H/V constraints restored")) {
    return false;
  }
  auto line_connects = [&](const auto& line, double x1, double y1, double x2,
                           double y2) {
    return (points_near(line.start_x, line.start_y, x1, y1) &&
            points_near(line.end_x, line.end_y, x2, y2)) ||
           (points_near(line.start_x, line.start_y, x2, y2) &&
            points_near(line.end_x, line.end_y, x1, y1));
  };
  return expect(line_connects(restored_a, 0.0, 0.0, 10.0, 0.0) &&
                    line_connects(restored_b, 10.0, 0.0, 10.0, 10.0),
                "undo: geometry restored in one step");
}

bool test_attached_line_follows() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_line(10.0, 0.0, 10.0, 10.0);
  const auto before = sketch_params(document);
  const std::string line_a = before.lines[0].id;
  const std::string line_b = before.lines[1].id;

  // Move only line A: the shared vertex (10,0) belongs to B too, so B's
  // far endpoint must follow the shared vertex.
  document = move(manager, {line_a}, 0.0, 5.0, 0.0, 0.0, 0.0);

  const auto after = sketch_params(document);
  const auto& moved_a = *std::find_if(
      after.lines.begin(), after.lines.end(),
      [&](const auto& line) { return line.id == line_a; });
  const auto& attached_b = *std::find_if(
      after.lines.begin(), after.lines.end(),
      [&](const auto& line) { return line.id == line_b; });

  if (!expect(points_near(moved_a.start_x, moved_a.start_y, 0.0, 5.0) &&
                  points_near(moved_a.end_x, moved_a.end_y, 10.0, 5.0),
              "attached: moved line translated by (0,5)")) {
    return false;
  }
  // B's shared endpoint follows to (10,5); its far endpoint stays (10,10).
  auto attached_start_shared =
      points_near(attached_b.start_x, attached_b.start_y, 10.0, 5.0) ||
      points_near(attached_b.end_x, attached_b.end_y, 10.0, 5.0);
  auto attached_far_kept =
      points_near(attached_b.start_x, attached_b.start_y, 10.0, 10.0) ||
      points_near(attached_b.end_x, attached_b.end_y, 10.0, 10.0);
  if (!expect(attached_start_shared && attached_far_kept,
              "attached: shared endpoint follows, far endpoint stays")) {
    return false;
  }
  // The shared vertex has a single position in the rebuilt vertex table.
  bool shared_id_matches = moved_a.start_vertex_id == attached_b.start_vertex_id ||
                           moved_a.start_vertex_id == attached_b.end_vertex_id ||
                           moved_a.end_vertex_id == attached_b.start_vertex_id ||
                           moved_a.end_vertex_id == attached_b.end_vertex_id;
  if (!expect(shared_id_matches, "attached: shared vertex id preserved")) {
    return false;
  }
  std::string shared_id;
  if (moved_a.start_vertex_id == attached_b.start_vertex_id ||
      moved_a.start_vertex_id == attached_b.end_vertex_id) {
    shared_id = moved_a.start_vertex_id;
  } else {
    shared_id = moved_a.end_vertex_id;
  }
  const SketchVertex* shared = find_vertex(after, shared_id);
  return expect(shared != nullptr &&
                    points_near(shared->x, shared->y, 10.0, 5.0),
                "attached: shared vertex at the moved position");
}

// A driving arc-radius dimension must survive a move with its value
// unchanged (the solver re-enforces it after the translation).
bool test_arc_radius_dim_survives_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_arc(
      10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  const auto before = sketch_params(document);
  const std::string arc_id = before.arcs[0].id;
  document = manager.add_sketch_arc_radius_dimension(arc_id);

  document = move(manager, {arc_id}, 5.0, 5.0, 0.0, 0.0, 0.0);

  const auto after = sketch_params(document);
  auto dimension = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& dim) { return dim.entity_id == arc_id; });
  if (!expect(dimension != after.dimensions.end(),
              "arc dim: dimension survives the move")) {
    return false;
  }
  if (!expect(!dimension->driven, "arc dim: dimension stays driving")) {
    return false;
  }
  return expect(std::abs(dimension->value - 10.0) < 1.0e-6,
                "arc dim: dimension value unchanged by translation");
}

// Regression: the document-layer sketch-tool whitelist
// (is_supported_sketch_tool) once rejected "move" with
// "Unsupported sketch tool: move" while the core-layer validate_tool
// accepted it.  set_sketch_tool must accept the tool end to end.
bool test_set_move_tool_accepted() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  bool threw = false;
  DocumentState document;
  try {
    document = manager.set_sketch_tool("move");
  } catch (const std::exception&) {
    threw = true;
  }
  if (!expect(!threw, "tool: set_sketch_tool(\"move\") must not throw")) {
    return false;
  }
  return expect(document.active_sketch_tool == "move",
                "tool: active_sketch_tool records \"move\"");
}

bool test_dimensions_resync() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  const auto before = sketch_params(document);
  const std::string line_id = before.lines[0].id;
  document = manager.add_sketch_line_length_dimension(line_id);

  document = move(manager, {line_id}, 0.0, 5.0, 0.0, 0.0, 0.0);

  const auto after = sketch_params(document);
  auto dimension = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& dim) { return dim.entity_id == line_id; });
  if (!expect(dimension != after.dimensions.end(),
              "dims: length dimension survives the move")) {
    return false;
  }
  if (!expect(!dimension->driven, "dims: dimension stays driving")) {
    return false;
  }
  return expect(std::abs(dimension->value - 10.0) < 1.0e-6,
                "dims: dimension value unchanged by translation");
}

}  // namespace

int main() {
  if (!test_translate_rectangle_rigid()) return 1;
  if (!test_move_keeps_fixed_vertex()) return 1;
  if (!test_rotate_strips_hv_and_preserves_geometry()) return 1;
  if (!test_translate_circle_and_arc()) return 1;
  if (!test_single_undo_step()) return 1;
  if (!test_attached_line_follows()) return 1;
  if (!test_dimensions_resync()) return 1;
  if (!test_arc_radius_dim_survives_move()) return 1;
  if (!test_set_move_tool_accepted()) return 1;

  std::cout << "sketch_move_test passed\n";
  return 0;
}
