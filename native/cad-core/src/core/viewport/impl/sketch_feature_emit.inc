    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      const auto profiles = detect_sketch_profiles(feature);

      // Emit Mirror tool preview geometry first so the rest of
      // the per-line bookkeeping (constraint badges, dimensions,
      // anchors) doesn't run for transient entities — they're
      // pure visual previews. The flag flips them to a dashed
      // translucent style on the UI side.
      if (feature.sketch_parameters->pending_mirror.has_value()) {
        for (const auto& preview_line :
             feature.sketch_parameters->pending_mirror->generated_lines) {
          auto primitive = make_sketch_line_primitive(
              preview_line, *feature.sketch_parameters, /*is_selected=*/false);
          primitive.is_preview = true;
          sketch_lines.push_back(primitive);
        }
        for (const auto& preview_circle :
             feature.sketch_parameters->pending_mirror->generated_circles) {
          auto primitive = make_sketch_circle_primitive(
              preview_circle,
              *feature.sketch_parameters,
              /*is_selected=*/false);
          primitive.is_preview = true;
          sketch_circles.push_back(primitive);
        }
      }

      const auto is_sketch_entity_selected = [&](const std::string& id) {
        if (!view->selected_sketch_entity_ids.empty()) {
          return std::find(view->selected_sketch_entity_ids.begin(),
                           view->selected_sketch_entity_ids.end(),
                           id) != view->selected_sketch_entity_ids.end();
        }
        return view->selected_sketch_entity_id.has_value() &&
               view->selected_sketch_entity_id.value() == id;
      };
      const auto is_sketch_vertex_selected = [&](const std::string& id) {
        if (!view->selected_sketch_vertex_ids.empty()) {
          return std::find(view->selected_sketch_vertex_ids.begin(),
                           view->selected_sketch_vertex_ids.end(),
                           id) != view->selected_sketch_vertex_ids.end();
        }
        return view->selected_sketch_vertex_id.has_value() &&
               view->selected_sketch_vertex_id.value() == id;
      };
      // Vertices owned by projection-generated entities are derived
      // geometry: no point spheres, no FIX badges (see
      // sketch_vertex_constraint_emit.inc). The projection records are
      // the source of truth — outline endpoints don't carry a "derived"
      // mark on the SketchVertex itself.
      std::unordered_set<std::string> projection_vertex_ids;
      const auto& sketch_params = *feature.sketch_parameters;
      for (const auto& projection : sketch_params.projections) {
        if (!projection.generated_vertex_id.empty()) {
          projection_vertex_ids.insert(projection.generated_vertex_id);
        }
        for (const auto& line_id : projection.generated_line_ids) {
          const auto it =
              std::find_if(sketch_params.lines.begin(),
                           sketch_params.lines.end(),
                           [&](const SketchLine& line) {
                             return line.id == line_id;
                           });
          if (it != sketch_params.lines.end()) {
            projection_vertex_ids.insert(it->start_vertex_id);
            projection_vertex_ids.insert(it->end_vertex_id);
          }
        }
        for (const auto& arc_id : projection.generated_arc_ids) {
          const auto it =
              std::find_if(sketch_params.arcs.begin(), sketch_params.arcs.end(),
                           [&](const SketchArc& arc) {
                             return arc.id == arc_id;
                           });
          if (it != sketch_params.arcs.end()) {
            projection_vertex_ids.insert(it->start_vertex_id);
            projection_vertex_ids.insert(it->end_vertex_id);
            projection_vertex_ids.insert(it->center_vertex_id);
          }
        }
        for (const auto& circle_id : projection.generated_circle_ids) {
          const auto it = std::find_if(
              sketch_params.circles.begin(), sketch_params.circles.end(),
              [&](const SketchCircle& circle) {
                return circle.id == circle_id;
              });
          if (it != sketch_params.circles.end()) {
            projection_vertex_ids.insert(it->center_vertex_id);
          }
        }
      }
      // Track which lines carry at least one relation badge so we
      // can offset the H/V badge to avoid overlap.
      std::unordered_set<std::string> lines_with_relation_badge;
      if (view->active_sketch_feature_id.has_value() &&
          view->active_sketch_feature_id.value() == feature.id) {
        for (const auto& relation : feature.sketch_parameters->line_relations) {
          lines_with_relation_badge.insert(relation.first_line_id);
          if (!relation.second_line_id.empty()) {
            lines_with_relation_badge.insert(relation.second_line_id);
          }
        }
      }

#include "core/viewport/impl/sketch_line_relation_emit.inc"
#include "core/viewport/impl/sketch_curve_polygon_emit.inc"
#include "core/viewport/impl/sketch_vertex_constraint_emit.inc"
#include "core/viewport/impl/sketch_profile_emit.inc"
    }
  }
