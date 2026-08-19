#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_operation.h"
#include "core/document/appearance.h"
#include "core/document/feature.h"
#include "core/document/parameter.h"
#include "core/sketch/sketch_types.h"

namespace polysmith::core {

struct DocumentState {
  std::string id;
  std::string name;
  std::string units;
  int revision;
  std::optional<std::string> selected_feature_id;
  std::optional<std::string> selected_reference_id;
  std::optional<std::string> selected_face_id;
  // Edge selection is a (possibly empty) ordered list rather than a
  // single optional id, so multi-edge fillet / chamfer selection is
  // first-class. Order is insertion order: the most recent click is
  // appended (or removed, when toggling). All other selection
  // categories remain single-id; widening them later is a small
  // change but unnecessary for current tooling.
  std::vector<std::string> selected_edge_ids;
  // Vertex selection is also a list (same insertion-order semantics
  // as edges) so the UI can show "distance between two vertices" in
  // the selection readout. Anything beyond two is permitted at the
  // storage layer; consumers that only handle pairs (e.g. the
  // distance display) read the first two entries.
  std::vector<std::string> selected_vertex_ids;
  std::optional<std::string> active_sketch_plane_id;
  std::optional<std::string> active_sketch_face_id;
  std::optional<std::string> active_sketch_feature_id;
  std::optional<std::string> active_sketch_tool;
  std::optional<std::string> selected_sketch_vertex_id;
  std::optional<std::string> selected_sketch_entity_id;
  std::vector<std::string> selected_sketch_vertex_ids;
  std::vector<std::string> selected_sketch_entity_ids;
  std::optional<std::string> selected_sketch_dimension_id;
  // Selected sketch text entity (glyph click in select mode reopens the
  // text panel for editing).
  std::optional<std::string> selected_sketch_text_id;
  std::optional<std::string> selected_sketch_profile_id;
  std::vector<std::string> selected_sketch_profile_ids;
  // Parametric timeline cursor. nullopt means the cursor is at the end
  // of history; otherwise the value is the number of non-root actions
  // included in the viewport rollback preview.
  std::optional<int> timeline_cursor;
  std::vector<FeatureEntry> feature_history;
  std::vector<ParameterEntry> parameters;
  DocumentAppearance appearance;
  SelectionFilter selection_filter;
  // CAM workspace data — setups, tool library, operations, post-processor,
  // and simulation state.  All CAM state lives here; see cam_types.h.
  CamDocumentData cam;
};

struct SessionState {
  int document_count;
  std::optional<std::string> active_document_id;
  bool can_undo;
  bool can_redo;
};

}  // namespace polysmith::core
