  if (payload.contains("line_relations") && payload.at("line_relations").is_array()) {
    for (const auto& relation_payload : payload.at("line_relations")) {
      polysmith::core::SketchLineRelation relation{};
      relation.id = read_string(relation_payload, "relation_id");
      relation.kind = read_string(relation_payload, "kind");
      relation.first_line_id = read_string(relation_payload, "first_line_id");
      relation.second_line_id = read_string(relation_payload, "second_line_id");
      params.line_relations.push_back(relation);
    }
  }
  // Older saves predate the general constraints array — silently default
  // to none.
  if (payload.contains("constraints") && payload.at("constraints").is_array()) {
    for (const auto& constraint_payload : payload.at("constraints")) {
      polysmith::core::SketchConstraint constraint{};
      constraint.constraint_id = read_string(constraint_payload, "constraint_id");
      constraint.kind = read_string(constraint_payload, "kind");
      if (constraint_payload.contains("target_ids") &&
          constraint_payload.at("target_ids").is_array()) {
        for (const auto& tid : constraint_payload.at("target_ids")) {
          constraint.target_ids.push_back(tid.get<std::string>());
        }
      }
      if (constraint_payload.contains("value") &&
          constraint_payload.at("value").is_number()) {
        constraint.value = constraint_payload.at("value").get<double>();
      }
      if (constraint_payload.contains("driven") &&
          constraint_payload.at("driven").is_boolean()) {
        constraint.driven = constraint_payload.at("driven").get<bool>();
      }
      params.constraints.push_back(constraint);
    }
  }
  // Older saves predate midpoint anchors — silently default to none.
  if (payload.contains("midpoint_anchors") &&
      payload.at("midpoint_anchors").is_array()) {
    for (const auto& anchor_payload : payload.at("midpoint_anchors")) {
      polysmith::core::SketchMidpointAnchor anchor{};
      anchor.id = read_string(anchor_payload, "anchor_id");
      anchor.point_id = read_string(anchor_payload, "point_id");
      anchor.line_id = read_string(anchor_payload, "line_id");
      params.midpoint_anchors.push_back(anchor);
    }
  }
  // Older saves predate the Project tool's standalone projected
  // points — absence of these keys just means no projections exist.
  if (payload.contains("projected_points") &&
      payload.at("projected_points").is_array()) {
    for (const auto& point_payload : payload.at("projected_points")) {
      polysmith::core::SketchProjectedPoint projected{};
      projected.id = read_string(point_payload, "point_id");
      projected.source_id = read_string(point_payload, "source_id");
      projected.x = read_number(point_payload, "x");
      projected.y = read_number(point_payload, "y");
      params.projected_points.push_back(projected);
    }
  }
  if (payload.contains("projected_sources") &&
      payload.at("projected_sources").is_array()) {
    for (const auto& source_payload : payload.at("projected_sources")) {
      if (!source_payload.is_string()) {
        continue;
      }
      params.projected_sources.push_back(source_payload.get<std::string>());
    }
  }
  // Live-link projection records. Saves predating the live-link work
  // (or a save that came from a build with the old face-only Project
  // tool) won't include this key — those documents lose the live
  // link until the user re-projects, which gracefully degrades to
  // the previous "frozen at projection time" behaviour.
  if (payload.contains("projections") &&
      payload.at("projections").is_array()) {
    for (const auto& projection_payload : payload.at("projections")) {
      polysmith::core::SketchProjection projection{};
      projection.id = read_string(projection_payload, "projection_id");
      projection.source_id = read_string(projection_payload, "source_id");
      projection.source_kind = read_string(projection_payload, "source_kind");
      if (projection_payload.contains("generated_line_ids") &&
          projection_payload.at("generated_line_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_line_ids")) {
          if (id.is_string()) {
            projection.generated_line_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_circle_ids") &&
          projection_payload.at("generated_circle_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_circle_ids")) {
          if (id.is_string()) {
            projection.generated_circle_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_arc_ids") &&
          projection_payload.at("generated_arc_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_arc_ids")) {
          if (id.is_string()) {
            projection.generated_arc_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_point_id") &&
          projection_payload.at("generated_point_id").is_string()) {
        projection.generated_point_id =
            projection_payload.at("generated_point_id").get<std::string>();
      }
      if (projection_payload.contains("dependency_broken") &&
          projection_payload.at("dependency_broken").is_boolean()) {
        projection.dependency_broken =
            projection_payload.at("dependency_broken").get<bool>();
      }
      if (projection_payload.contains("dependency_warning") &&
          projection_payload.at("dependency_warning").is_string()) {
        projection.dependency_warning =
            projection_payload.at("dependency_warning").get<std::string>();
      }
      params.projections.push_back(std::move(projection));
    }
  }
  // Older saves predate point-line anchors — silently default to none.
  if (payload.contains("point_line_anchors") &&
      payload.at("point_line_anchors").is_array()) {
    for (const auto& anchor_payload : payload.at("point_line_anchors")) {
      polysmith::core::SketchVertexLineAnchor anchor{};
      anchor.id = read_string(anchor_payload, "anchor_id");
      anchor.point_id = read_string(anchor_payload, "point_id");
      anchor.line_id = read_string(anchor_payload, "line_id");
      anchor.t = anchor_payload.contains("t") &&
                         anchor_payload.at("t").is_number()
                     ? anchor_payload.at("t").get<double>()
                     : 0.5;
      params.point_line_anchors.push_back(anchor);
    }
  }
