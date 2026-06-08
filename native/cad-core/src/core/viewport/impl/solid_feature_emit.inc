  for (const auto& feature : view->feature_history) {
    // Suppressed features are excluded from every viewport-visible
    // emission path: legacy primitives, sketch overlays, body-derived
    // faces (already excluded via body_compiler skipping them). The UI
    // still shows the feature in the timeline / hierarchy, just dimmed.
    if (feature.suppressed) {
      continue;
    }
    const bool is_selected =
        view->selected_feature_id.has_value() &&
        view->selected_feature_id.value() == feature.id;
    const bool feature_consumed_by_boolean =
        consumed.find(feature.id) != consumed.end();

#include "core/viewport/impl/primitive_solid_feature_emit.inc"
#include "core/viewport/impl/extrude_feature_emit.inc"
