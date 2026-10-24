#include <chrono>
#include <string>

#include "core/diagnostics/logger.h"
#include "core/document/impl/private_linked_extrude_refresh.inc"
#include "core/document/impl/private_linked_loft_refresh.inc"
#include "core/document/impl/private_linked_revolve_refresh.inc"
#include "core/document/impl/private_linked_sweep_refresh.inc"

void refresh_linked_extrudes(DocumentState &document,
                             const FeatureEntry &sketch_feature) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return;
  }

  const auto refresh_start = std::chrono::steady_clock::now();
  refresh_linked_extrude_features(document, sketch_feature);
  refresh_linked_loft_features(document, sketch_feature);
  refresh_linked_revolve_features(document, sketch_feature);
  refresh_linked_sweep_features(document, sketch_feature);

  const auto refresh_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - refresh_start)
                              .count();
  if (refresh_ms >= 100) {
    // Timing evidence: this runs on EVERY sketch edit, and it re-derives every
    // dependent feature (profile re-detection + OCCT booleans). If the linked
    // features, not the solver, are the second, this line says so.
    polysmith::core::log_warn(
        "sketch", "linked feature refresh took " + std::to_string(refresh_ms) +
                      " ms on sketch " + sketch_feature.id);
  }
}
