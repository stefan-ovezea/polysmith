void trim_sketch_entity(FeatureEntry& feature,
                        const std::string& entity_id,
                        double click_x,
                        double click_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can trim entities");
  }

  auto& params = *feature.sketch_parameters;


#include "core/sketch/impl/trim_line_entity_command.inc"
#include "core/sketch/impl/trim_circle_entity_command.inc"
#include "core/sketch/impl/trim_arc_entity_command.inc"

  throw std::runtime_error("Sketch entity not found: " + entity_id);
}
