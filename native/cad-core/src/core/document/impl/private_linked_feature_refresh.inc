#include "core/document/impl/private_linked_extrude_refresh.inc"
#include "core/document/impl/private_linked_loft_refresh.inc"
#include "core/document/impl/private_linked_revolve_refresh.inc"
#include "core/document/impl/private_linked_sweep_refresh.inc"

void refresh_linked_extrudes(DocumentState &document,
                             const FeatureEntry &sketch_feature) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return;
  }

  refresh_linked_extrude_features(document, sketch_feature);
  refresh_linked_loft_features(document, sketch_feature);
  refresh_linked_revolve_features(document, sketch_feature);
  refresh_linked_sweep_features(document, sketch_feature);
}
