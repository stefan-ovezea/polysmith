// Next free numeric suffix for a new entity id: one past the highest
// trailing integer among the existing ids.  The old size-based scheme
// (entities.size() + 1) reuses ids after trims delete entities — the
// wire builder and profile logic treat ids as unique keys, so a
// duplicated id corrupts wires and faces (open wire -> uncapped shells
// on extrude).
int next_trim_entity_index(const std::vector<std::string>& ids) {
  int highest = 0;
  for (const auto& id : ids) {
    const size_t dash = id.rfind('-');
    if (dash == std::string::npos || dash + 1 >= id.size()) {
      continue;
    }
    const std::string suffix = id.substr(dash + 1);
    if (suffix.find_first_not_of("0123456789") != std::string::npos) {
      continue;
    }
    highest = std::max(highest, std::stoi(suffix));
  }
  return highest + 1;
}

void trim_sketch_entity(FeatureEntry& feature,
                        const std::string& entity_id,
                        double click_x,
                        double click_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can trim entities");
  }

  auto& params = *feature.sketch_parameters;

  // Generated glyph segments are derived geometry — trimming them would
  // open the contour and the expansion pass would just regenerate them.
  ensure_user_editable_entity(params, entity_id);


#include "core/sketch/impl/trim_line_entity_command.inc"
#include "core/sketch/impl/trim_circle_entity_command.inc"
#include "core/sketch/impl/trim_arc_entity_command.inc"

  throw std::runtime_error("Sketch entity not found: " + entity_id);
}
