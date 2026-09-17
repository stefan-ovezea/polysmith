// ================================================================
// Drag-paint trim (R5) — one stroke trims every crossed entity.
//
// Each entry goes through the ordinary single-entity trim with the
// segment RE-DERIVED from the click point (no segment_index): earlier
// entries in the stroke may already have split or shortened later
// entities, so a preview-cached index would be stale. Per-entry
// failures (an entity consumed by an earlier trim, a click that no
// longer lands on a segment) are logged and skipped — the stroke
// commits whatever succeeded. The document layer pushes ONE undo
// state for the whole stroke.
// ================================================================

void trim_sketch_stroke(FeatureEntry& feature,
                        const std::vector<TrimStrokeEntry>& entries,
                        std::vector<std::string>* minted_vertex_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can trim entities");
  }
  if (entries.empty()) return;

  for (const auto& entry : entries) {
    try {
      trim_sketch_entity(feature, entry.entity_id, entry.click_x,
                         entry.click_y, std::nullopt, minted_vertex_ids);
    } catch (const std::exception& e) {
      // One bad segment must not sink the whole stroke: the entity may
      // have been consumed or re-shaped by an earlier entry.
      log_warn("trim",
               "stroke entry skipped for '" + entry.entity_id + "': " +
                   e.what());
    }
  }
}
