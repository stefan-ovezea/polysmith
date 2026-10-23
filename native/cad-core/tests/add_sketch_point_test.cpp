// Standalone sketch point (parity: T1.4).
//
// A Point-tool point is stored as a `SketchPoint` record and re-emitted into
// the vertex table by every `rebuild_sketch_vertices` pass with kind "point".
// Its id IS the vertex id, so selection, snapping, dimensions and constraints
// address it exactly like a line endpoint. It must never change the region set
// (a point bounds nothing), must stay movable, and must survive a save/load
// round trip through the `points` payload array.
//
// Profile-set assertions use profiles_match (complete region-set matching).

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/viewport/viewport.h"
#include "protocol/serialization.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchVertex;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

const SketchVertex* find_vertex(const SketchFeatureParameters& params,
                                const std::string& id) {
  for (const auto& vertex : params.vertices) {
    if (vertex.id == id) {
      return &vertex;
    }
  }
  return nullptr;
}

bool test_point_is_a_movable_vertex_and_no_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_point(10.0, 20.0);
  const auto params = sketch_params(document);

  if (!expect(params.points.size() == 1, "point: one SketchPoint record")) {
    return false;
  }
  const std::string point_id = params.points[0].id;
  if (!expect(!point_id.empty(), "point: record carries a vertex id")) {
    return false;
  }
  const SketchVertex* vertex = find_vertex(params, point_id);
  if (!expect(vertex != nullptr, "point: re-emitted into the vertex table")) {
    return false;
  }
  if (!expect(vertex->kind == "point", "point: vertex kind is \"point\"")) {
    return false;
  }
  if (!expect(!vertex->is_fixed, "point: movable (not fixed)")) {
    return false;
  }
  if (!expect(std::abs(vertex->x - 10.0) < 1e-9 &&
                  std::abs(vertex->y - 20.0) < 1e-9,
              "point: vertex sits at the requested position")) {
    return false;
  }
  if (!expect(params.lines.empty() && params.arcs.empty() &&
                  params.circles.empty(),
              "point: creates no curve geometry")) {
    return false;
  }
  std::string reason;
  if (!expect(profiles_match(document, {}, &reason),
              ("point: adds no profile region (" + reason + ")").c_str())) {
    return false;
  }
  return expect(document.active_sketch_tool == "point",
                "point: tool stays armed as \"point\"");
}

bool test_point_inside_a_rectangle_keeps_the_region_set() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);

  document = manager.add_sketch_point(10.0, 8.0);
  const auto after = sketch_params(document);

  // Exactly the four rectangle sides still bound the only region.
  std::string reason;
  if (!expect(profiles_match(
                  document,
                  {{{before.lines[0].id, before.lines[1].id, before.lines[2].id,
                     before.lines[3].id},
                    "polygon",
                    false}},
                  &reason),
              ("point in rectangle: region set unchanged (" + reason + ")")
                  .c_str())) {
    return false;
  }
  if (!expect(after.points.size() == 1,
              "point in rectangle: point record present")) {
    return false;
  }
  // Four corners + the standalone point.
  if (!expect(after.vertices.size() == 5,
              "point in rectangle: vertex table has 4 corners + 1 point")) {
    return false;
  }
  const SketchVertex* point = find_vertex(after, after.points[0].id);
  return expect(point != nullptr && point->kind == "point",
                "point in rectangle: point vertex present and typed");
}

bool test_moving_a_point_updates_its_record() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_point(5.0, 5.0);
  const std::string point_id = sketch_params(document).points[0].id;

  // The vertex table is authoritative while dragging; the record must follow
  // it, or the next refresh would snap the point back to its creation spot.
  document = manager.update_sketch_vertex(point_id, 30.0, 40.0);
  const auto params = sketch_params(document);

  if (!expect(params.points.size() == 1, "point move: still one record")) {
    return false;
  }
  if (!expect(std::abs(params.points[0].x - 30.0) < 1e-9 &&
                  std::abs(params.points[0].y - 40.0) < 1e-9,
              "point move: stored coordinates follow the vertex")) {
    return false;
  }
  const SketchVertex* vertex = find_vertex(params, point_id);
  return expect(vertex != nullptr && std::abs(vertex->x - 30.0) < 1e-9 &&
                    std::abs(vertex->y - 40.0) < 1e-9,
                "point move: vertex keeps the dragged position");
}

bool test_duplicate_click_selects_the_existing_point() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_point(10.0, 10.0);
  const std::string first_id = sketch_params(document).points[0].id;
  const long revision = document.revision;

  // Clicking the same spot twice must not stack two coincident dots.
  document = manager.add_sketch_point(10.0004, 10.0004);
  const auto params = sketch_params(document);
  return expect(params.points.size() == 1,
                "point dedup: a repeat click creates nothing") &&
         expect(document.revision == revision,
                "point dedup: a repeat click does not bump the revision") &&
         expect(document.selected_sketch_vertex_id.has_value() &&
                    document.selected_sketch_vertex_id.value() == first_id,
                "point dedup: the existing point becomes the selection");
}

bool test_deleting_the_point_removes_only_it() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  document = manager.add_sketch_point(10.0, 8.0);
  const auto added = sketch_params(document);
  const std::string point_id = added.points[0].id;

  document = manager.delete_sketch_selection({}, {point_id}, {});
  const auto params = sketch_params(document);

  if (!expect(params.points.empty(), "point delete: record removed")) {
    return false;
  }
  if (!expect(find_vertex(params, point_id) == nullptr,
              "point delete: vertex removed")) {
    return false;
  }
  if (!expect(params.vertices.size() == 4,
              "point delete: the four rectangle corners survive")) {
    return false;
  }
  std::string reason;
  return expect(profiles_match(
                    document,
                    {{{params.lines[0].id, params.lines[1].id,
                       params.lines[2].id, params.lines[3].id},
                      "polygon",
                      false}},
                    &reason),
                ("point delete: region set unchanged (" + reason + ")")
                    .c_str());
}

bool test_point_survives_save_and_load() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_point(7.0, 9.0);
  const std::string point_id = sketch_params(document).points[0].id;

  const std::string path =
      (std::filesystem::temp_directory_path() /
       "polysmith_add_sketch_point_test.polysmith")
          .string();
  {
    std::ofstream stream(path);
    stream << polysmith::protocol::to_payload(document).dump();
  }

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  DocumentState loaded = loaded_manager.load_document_from_path(path);
  auto loaded_params = sketch_params(loaded);
  std::filesystem::remove(path);

  if (!expect(loaded_params.points.size() == 1,
              "point load: the points record round-trips")) {
    return false;
  }
  // The load path re-derives the sketch, so the point must be back in the
  // vertex table with its kind — not merely parsed.
  const SketchVertex* vertex = find_vertex(loaded_params, point_id);
  return expect(vertex != nullptr && vertex->kind == "point",
                "point load: the vertex is re-emitted after loading");
}

bool test_point_is_a_snap_target_in_viewport_state() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_point(12.0, 8.0);
  const std::string point_id = sketch_params(document).points[0].id;

  // Dragging a line endpoint onto a placed point is the point of the tool; the
  // drag only knows about the snap candidates in the viewport snapshot, so the
  // point MUST be emitted there (nothing in the document reaches the UI's
  // snap resolver).
  const auto viewport = polysmith::core::build_viewport_state(
      std::optional<DocumentState>{document});
  const polysmith::core::SnapCandidate* point_candidate = nullptr;
  for (const auto& candidate : viewport.snap_candidates) {
    if (candidate.kind == "endpoint" && candidate.vertex_id == point_id) {
      point_candidate = &candidate;
      break;
    }
  }
  if (!expect(point_candidate != nullptr,
              "point snap: an endpoint candidate exists for the point")) {
    return false;
  }
  return expect(std::abs(point_candidate->local_x - 12.0) < 1e-9 &&
                    std::abs(point_candidate->local_y - 8.0) < 1e-9,
                "point snap: the candidate sits at the point");
}

bool test_dropping_a_line_on_a_point_absorbs_the_point() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  document = manager.add_sketch_point(10.0, 8.0);
  const auto before = sketch_params(document);
  const std::string point_id = before.points[0].id;
  const std::string dragged_id = before.lines[0].start_vertex_id;

  // Drag a rectangle corner onto the point with the bind flag — the gesture a
  // snap-then-drop performs.
  document = manager.update_sketch_vertex(dragged_id, 10.0, 8.0, point_id);
  const auto params = sketch_params(document);

  if (!expect(params.points.empty(),
              "bind: the Point record is absorbed by the drop")) {
    return false;
  }
  if (!expect(find_vertex(params, point_id) == nullptr,
              "bind: the standalone point vertex is gone")) {
    return false;
  }
  // Four corners, one of which now sits on the point: no fifth "hidden" dot.
  if (!expect(params.vertices.size() == 4,
              "bind: the vertex table still has exactly 4 corners")) {
    return false;
  }
  const SketchVertex* dropped = find_vertex(params, dragged_id);
  if (!expect(dropped != nullptr &&
                  std::abs(dropped->x - 10.0) < 1e-9 &&
                  std::abs(dropped->y - 8.0) < 1e-9,
              "bind: the dropped vertex sits on the point")) {
    return false;
  }
  const bool has_coincident =
      std::any_of(params.constraints.begin(), params.constraints.end(),
                  [](const polysmith::core::SketchConstraint& constraint) {
                    return constraint.kind == "coincident";
                  });
  if (!expect(has_coincident,
              "bind: a coincident constraint records the binding")) {
    return false;
  }
  std::string reason;
  if (!expect(profiles_match(
                  document,
                  {{{params.lines[0].id, params.lines[1].id, params.lines[2].id,
                     params.lines[3].id},
                    "polygon",
                    false}},
                  &reason),
              ("bind: the region set is unchanged (" + reason + ")").c_str())) {
    return false;
  }

  // ONE undo restores both the point and the corner position (move + bind are
  // one command).
  document = manager.undo();
  const auto undone = sketch_params(document);
  if (!expect(undone.points.size() == 1,
              "bind undo: the point comes back")) {
    return false;
  }
  const SketchVertex* restored = find_vertex(undone, dragged_id);
  return expect(restored != nullptr &&
                    std::abs(restored->x - 0.0) < 1e-9 &&
                    std::abs(restored->y - 0.0) < 1e-9,
                "bind undo: the corner returns to its pre-drag position");
}

}  // namespace

int main() {
  if (!test_point_is_a_movable_vertex_and_no_profile()) return 1;
  if (!test_point_inside_a_rectangle_keeps_the_region_set()) return 1;
  if (!test_moving_a_point_updates_its_record()) return 1;
  if (!test_duplicate_click_selects_the_existing_point()) return 1;
  if (!test_deleting_the_point_removes_only_it()) return 1;
  if (!test_point_survives_save_and_load()) return 1;
  if (!test_point_is_a_snap_target_in_viewport_state()) return 1;
  if (!test_dropping_a_line_on_a_point_absorbs_the_point()) return 1;

  std::cout << "add_sketch_point_test passed\n";
  return 0;
}
