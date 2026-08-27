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
                        double click_y,
                        std::optional<int> segment_index,
                        std::vector<std::string>* minted_vertex_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can trim entities");
  }

  auto& params = *feature.sketch_parameters;

  // Every vertex id the trim creates goes through this helper so the
  // document layer can collect them and freeze the fresh split points
  // against the planegcs pass — a solver-moved split point re-opens
  // the loop the trim just welded.
  auto mint_vertex_id = [&]() -> std::string {
    const std::string id =
        "vertex-" + std::to_string(params.next_vertex_index++);
    if (minted_vertex_ids != nullptr) minted_vertex_ids->push_back(id);
    return id;
  };

  // Generated glyph segments are derived geometry — trimming them would
  // open the contour and the expansion pass would just regenerate them.
  ensure_user_editable_entity(params, entity_id);

  // The preview's hovered segment wins over a re-derived click segment:
  // both come from the same split, so passing the index through makes
  // the red highlight and the trim agree by construction instead of
  // racing near segment boundaries.
  auto resolve_clicked_index =
      [&](const std::vector<TrimSegment>& segments, int derived_index) {
        if (segment_index.has_value() && *segment_index >= 0 &&
            *segment_index < static_cast<int>(segments.size())) {
          return *segment_index;
        }
        return derived_index;
      };


#include "core/sketch/impl/trim_line_entity_command.inc"
#include "core/sketch/impl/trim_circle_entity_command.inc"
#include "core/sketch/impl/trim_arc_entity_command.inc"

  throw std::runtime_error("Sketch entity not found: " + entity_id);
}
