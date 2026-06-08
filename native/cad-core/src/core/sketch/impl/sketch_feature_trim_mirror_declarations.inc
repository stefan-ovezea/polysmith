// ---------------------------------------------------------------
// Mirror tool — contextual modeling pending preview lifecycle.
//
// The mirror feature follows the canonical action pattern (see
// `wiki/polysmith.wiki/Contextual-Modeling-Workflow.md`):
//   1. `start_mirror_preview` — opens a transient `pending_mirror`
//      with empty selections. No geometry yet.
//   2. `update_mirror_preview_axis` / `update_mirror_preview_objects`
//      — set parameters; preview geometry is regenerated each time
//      so the viewport snapshot reflects the current selection.
//   3. `commit_mirror_preview` — moves the generated geometry into
//      the sketch's main arrays as real entities (with dimensions
//      and inferred constraints), and clears `pending_mirror`.
//   4. `cancel_mirror_preview` — discards the generated geometry
//      and clears `pending_mirror`. No state escapes the tool.
// ---------------------------------------------------------------
// Sketch trim tool — line trimming (Phase 1).
//
// Given a sketch entity id and a click position in sketch-local
// coordinates, find all intersections between the target entity and
// other non-construction sketch entities, split the target at the
// intersection points, identify the segment under the click, and
// update the target to keep only that segment. The trimmed-away
// portions are discarded. Phase 1 handles lines only.
//
// Throws when the entity is not found or the click position does not
// correspond to any segment. Refresh is NOT called here — the
// caller (document layer) handles that.
void trim_sketch_entity(FeatureEntry& feature,
                        const std::string& entity_id,
                        double click_x,
                        double click_y);

// ---------------------------------------------------------------
void start_mirror_preview(FeatureEntry& feature);
void update_mirror_preview_axis(FeatureEntry& feature,
                                const std::string& axis_line_id);
void update_mirror_preview_objects(
    FeatureEntry& feature,
    const std::vector<std::string>& object_ids);
void commit_mirror_preview(FeatureEntry& feature,
                           int& next_line_index,
                           int& next_circle_index,
                           bool persistent = false);
void cancel_mirror_preview(FeatureEntry& feature);
