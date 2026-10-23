// Guard test for the `sketch_entities_changed` partial reply.
//
// The partial event carries only line endpoints and vertex coordinates. It
// must therefore be suppressed for ANY edit that changes something else —
// arcs, circles, fillets, dimension values, ... — because the client would
// otherwise keep a stale copy. The regression that motivated this guard:
// after a fillet, the UI's sketch still held the pre-fillet arcs, and the
// planegcs bridge refused an endpoint drag with
// "sketch object vertex-N not found" (an uncaught UI error).
//
// `sketch_partial_event_safe` is the gate; it compares the serialized
// non-line sketch state, so a future entity kind is covered automatically.

#include <cmath>
#include <iostream>
#include <string>

#include "app/command_context.h"
#include "core/document/document.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

FeatureEntry sketch_feature(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      return feature;
    }
  }
  throw std::runtime_error("test: no sketch feature");
}

bool test_construction_edit_stays_partial() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0,
                                   /*is_construction=*/true);
  const FeatureEntry before = sketch_feature(document);

  // A construction rectangle has no automatic dimensions, so moving a corner
  // changes only vertices + line endpoints — exactly what the partial event
  // transports.
  const std::string vertex =
      before.sketch_parameters->lines[0].end_vertex_id;
  document = manager.update_sketch_vertex(vertex, 39.0, 1.0);
  const FeatureEntry after = sketch_feature(document);

  return expect(polysmith::sketch_partial_event_safe(before, after),
                "partial event: a vertex-only edit is safe to patch");
}

bool test_dimension_value_change_forces_full_state() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const FeatureEntry before = sketch_feature(document);

  // Moving a corner changes the automatic length dimensions' values; the
  // partial event has no dimension payload, so it must not be used.
  const std::string vertex =
      before.sketch_parameters->lines[0].end_vertex_id;
  document = manager.update_sketch_vertex(vertex, 39.0, 1.0);
  const FeatureEntry after = sketch_feature(document);

  return expect(!polysmith::sketch_partial_event_safe(before, after),
                "partial event: changed dimension values force a full state");
}

bool test_fillet_creation_forces_full_state() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const FeatureEntry before = sketch_feature(document);
  const auto& lines = before.sketch_parameters->lines;

  document = manager.add_sketch_fillet(lines[0].end_vertex_id, lines[0].id,
                                       lines[1].id, 3.0);
  const FeatureEntry after = sketch_feature(document);

  // The new arc + fillet record are not in the partial payload — this is the
  // exact shape that produced the "vertex-N not found" crash.
  return expect(!polysmith::sketch_partial_event_safe(before, after),
                "partial event: adding a fillet (arc + record) needs a full "
                "state");
}

bool test_circle_center_move_forces_full_state() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  // Construction circle: no automatic radius dimension, so the circle's own
  // cached geometry is the only thing that changes.
  DocumentState document =
      manager.add_sketch_circle(10.0, 10.0, 3.0, /*is_construction=*/true);
  const FeatureEntry before = sketch_feature(document);
  const std::string center =
      before.sketch_parameters->circles[0].center_vertex_id;

  document = manager.update_sketch_vertex(center, 12.0, 11.0);
  const FeatureEntry after = sketch_feature(document);

  return expect(!polysmith::sketch_partial_event_safe(before, after),
                "partial event: moving a circle centre needs a full state "
                "(the arc/circle geometry is not in the patch)");
}

bool test_adding_a_construction_line_stays_partial() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const FeatureEntry before = sketch_feature(document);

  document = manager.add_sketch_line(0.0, 10.0, 40.0, 10.0,
                                     /*is_construction=*/true);
  const FeatureEntry after = sketch_feature(document);

  // A construction line adds one line + two vertices and nothing else; the
  // patch carries both new lines and vertices, so this stays on the fast path.
  return expect(polysmith::sketch_partial_event_safe(before, after),
                "partial event: a construction line can still be patched");
}

}  // namespace

int main() {
  if (!test_construction_edit_stays_partial()) return 1;
  if (!test_dimension_value_change_forces_full_state()) return 1;
  if (!test_fillet_creation_forces_full_state()) return 1;
  if (!test_circle_center_move_forces_full_state()) return 1;
  if (!test_adding_a_construction_line_stays_partial()) return 1;

  std::cout << "sketch_partial_event_test passed\n";
  return 0;
}
