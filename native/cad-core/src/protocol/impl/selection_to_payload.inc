// Selection-only payload: what a select_* command actually changes.
//
// Selection is INTERACTION state (CLAUDE.md: "core sends document state, not
// interaction state"), but it was shipped by re-sending the whole document —
// 1,219 KB on a dense sketch for a change that flips one id, which is what made
// clicking a surface feel broken.  The core still OWNS the selection (commands
// act on it); this is only the transport, and it carries the ids so a future
// move to UI-owned selection needs no new wire format.
json to_selection_payload(const polysmith::core::DocumentState& document) {
  json profile_ids = json::array();
  for (const auto& id : document.selected_sketch_profile_ids) {
    profile_ids.push_back(id);
  }
  return {
      {"revision", document.revision},
      {"selected_feature_id",
       document.selected_feature_id.has_value()
           ? json(document.selected_feature_id.value())
           : json(nullptr)},
      {"selected_reference_id",
       document.selected_reference_id.has_value()
           ? json(document.selected_reference_id.value())
           : json(nullptr)},
      {"selected_face_id",
       document.selected_face_id.has_value()
           ? json(document.selected_face_id.value())
           : json(nullptr)},
      {"selected_edge_ids", document.selected_edge_ids},
      {"selected_vertex_ids", document.selected_vertex_ids},
      {"selected_sketch_vertex_id",
       document.selected_sketch_vertex_id.has_value()
           ? json(document.selected_sketch_vertex_id.value())
           : json(nullptr)},
      {"selected_sketch_entity_id",
       document.selected_sketch_entity_id.has_value()
           ? json(document.selected_sketch_entity_id.value())
           : json(nullptr)},
      {"selected_sketch_vertex_ids", document.selected_sketch_vertex_ids},
      {"selected_sketch_entity_ids", document.selected_sketch_entity_ids},
      {"selected_sketch_dimension_id",
       document.selected_sketch_dimension_id.has_value()
           ? json(document.selected_sketch_dimension_id.value())
           : json(nullptr)},
      {"selected_sketch_text_id",
       document.selected_sketch_text_id.has_value()
           ? json(document.selected_sketch_text_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_id",
       document.selected_sketch_profile_id.has_value()
           ? json(document.selected_sketch_profile_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_ids", profile_ids},
  };
}
