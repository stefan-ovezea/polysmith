    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      if (feature.dependency_broken || feature.status == "warning") {
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
