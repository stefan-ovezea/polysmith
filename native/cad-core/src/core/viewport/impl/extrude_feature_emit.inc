    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      // A warning status alone must not hide the primitive (S30b).
      if (feature.dependency_broken) {
        continue;
      }
      if (feature_consumed_by_boolean) {
        continue;
      }
      const auto& parameters = feature.extrude_parameters.value();


#include "core/viewport/impl/extrude_rectangle_emit.inc"
#include "core/viewport/impl/extrude_polygon_emit.inc"
#include "core/viewport/impl/extrude_circle_emit.inc"
      continue;
    }
