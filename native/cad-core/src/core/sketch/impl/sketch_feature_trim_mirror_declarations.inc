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
// segment_index: when set, the trim deletes THAT segment of the entity
// (as reported by the trim_preview command) instead of re-selecting the
// segment from the click position. The preview and the trim must agree —
// re-selecting from a slightly different click near a segment boundary
// is exactly how the red highlight ended up showing one segment while
// the trim deleted another.
void trim_sketch_entity(FeatureEntry& feature,
                        const std::string& entity_id,
                        double click_x,
                        double click_y,
                        std::optional<int> segment_index = std::nullopt,
                        std::vector<std::string>* minted_vertex_ids = nullptr);

// Extend tool. Extends the entity (line or arc) from the end nearest
// the click to the nearest intersection with another
// non-construction entity. Throws when there is no intersection in
// the extension direction. Refresh is NOT called here — the caller
// (document layer) handles that.
void extend_sketch_entity(FeatureEntry& feature,
                          const std::string& entity_id,
                          double click_x,
                          double click_y);

// Offset tool. Creates a NEW entity at the signed distance from the
// source (line -> parallel line, circle -> concentric circle with
// radius + distance, arc -> same sweep at radius + distance).
// Throws when the offset would collapse/invert a circle or arc.
void offset_sketch_entity(FeatureEntry& feature,
                          const std::string& entity_id,
                          double distance);

// ---------------------------------------------------------------
// Corner trim (R2). Trims/extends two entities (lines or arcs) to
// their virtual corner: the intersection of their underlying curves
// nearest the click. Each entity keeps the portion from its far end
// (the end farther from the click) to the corner. Throws when there
// is no intersection (parallel lines), the corner would collapse an
// entity, or an entity is not a line/arc.
// ---------------------------------------------------------------
struct CornerTrimPreviewSegment {
  std::string kind;  // "line" | "arc"
  double sx = 0.0, sy = 0.0, ex = 0.0, ey = 0.0;
  // Arc rendering extras (valid when kind == "arc"):
  double center_x = 0.0, center_y = 0.0, radius = 0.0;
  bool ccw = true;
};

struct CornerTrimPreview {
  bool valid = false;
  double corner_x = 0.0;
  double corner_y = 0.0;
  CornerTrimPreviewSegment a;
  CornerTrimPreviewSegment b;
};

// Hover preview for the second pick: the corner point + the
// resulting trimmed/extended segments of both entities, without
// mutating anything. entity_b_id is the hover candidate; cursor is
// the mouse position.
CornerTrimPreview corner_trim_preview(const FeatureEntry& feature,
                                      const std::string& entity_a_id,
                                      const std::string& entity_b_id,
                                      double cursor_x,
                                      double cursor_y);

void corner_trim_sketch_entities(FeatureEntry& feature,
                                 const std::string& entity_a_id,
                                 const std::string& entity_b_id,
                                 double click_x,
                                 double click_y);

// ---------------------------------------------------------------
// Drag-paint trim (R5). One stroke trims every entity the pointer
// crossed. Entries carry the crossed entity + the click point; the
// segment is re-derived from the CURRENT geometry at execution time
// (earlier entries in the stroke may already have mutated it).
// Per-entry failures are logged and skipped — the stroke commits
// whatever succeeded as ONE undo entry (document layer).
// Refresh is NOT called here — the caller (document layer) handles
// that; minted_vertex_ids collects the fresh split point ids across
// every entry for the freeze pass.
// ---------------------------------------------------------------
struct TrimStrokeEntry {
  std::string entity_id;
  double click_x = 0.0;
  double click_y = 0.0;
};

void trim_sketch_stroke(FeatureEntry& feature,
                        const std::vector<TrimStrokeEntry>& entries,
                        std::vector<std::string>* minted_vertex_ids);

// ---------------------------------------------------------------
// Split (R4). Divides the clicked entity at ALL its intersections
// without deleting anything (lines/arcs/partial ellipses/splines).
// Circles and full ellipses need TWO split points (SolidWorks rule):
// they split into two arcs at (click_x, click_y) and
// (split2_x, split2_y). Throws when the entity cannot be split.
// Refresh is NOT called here — the caller (document layer) handles
// that; minted_vertex_ids collects the fresh split point ids for the
// freeze pass.
// ---------------------------------------------------------------
void split_sketch_entity(FeatureEntry& feature,
                         const std::string& entity_id,
                         double click_x,
                         double click_y,
                         double split2_x,
                         double split2_y,
                         std::vector<std::string>* minted_vertex_ids);

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
