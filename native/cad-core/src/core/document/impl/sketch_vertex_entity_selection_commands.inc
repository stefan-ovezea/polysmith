DocumentState DocumentManager::select_sketch_vertex(const std::string& vertex_id,
                                                   bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto vertex_it = std::find_if(
      feature_it->sketch_parameters->vertices.begin(),
      feature_it->sketch_parameters->vertices.end(),
      [&](const SketchVertex& vtx) { return vtx.id == vertex_id; });
  if (vertex_it == feature_it->sketch_parameters->vertices.end()) {
    throw std::runtime_error("Sketch vertex not found: " + vertex_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  if (additive) {
    auto& vertex_ids = document_->selected_sketch_vertex_ids;
    const auto existing =
        std::find(vertex_ids.begin(), vertex_ids.end(), vertex_id);
    if (existing == vertex_ids.end()) {
      vertex_ids.push_back(vertex_id);
    } else {
      vertex_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_vertex_ids = {vertex_id};
  }
  document_->selected_sketch_vertex_id =
      document_->selected_sketch_vertex_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_vertex_ids.back());
  return document_.value();
}

DocumentState DocumentManager::select_sketch_entity(
    const std::string& entity_id, bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const bool has_line = std::any_of(
      feature_it->sketch_parameters->lines.begin(),
      feature_it->sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });
  const bool has_circle = std::any_of(
      feature_it->sketch_parameters->circles.begin(),
      feature_it->sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == entity_id; });
  const bool has_arc = std::any_of(
      feature_it->sketch_parameters->arcs.begin(),
      feature_it->sketch_parameters->arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });
  const bool has_polygon = std::any_of(
      feature_it->sketch_parameters->polygons.begin(),
      feature_it->sketch_parameters->polygons.end(),
      [&](const SketchPolygon& polygon) { return polygon.id == entity_id; });

  if (!has_line && !has_circle && !has_arc && !has_polygon) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  const bool is_projected_entity = std::any_of(
      feature_it->sketch_parameters->projections.begin(),
      feature_it->sketch_parameters->projections.end(),
      [&](const SketchProjection& projection) {
        return projection_generates_entity(projection, entity_id);
      });

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  if (additive) {
    auto& entity_ids = document_->selected_sketch_entity_ids;
    const auto existing =
        std::find(entity_ids.begin(), entity_ids.end(), entity_id);
    if (existing == entity_ids.end()) {
      entity_ids.push_back(entity_id);
    } else {
      entity_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_entity_ids = {entity_id};
  }
  document_->selected_sketch_entity_id =
      document_->selected_sketch_entity_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_entity_ids.back());
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return !is_projected_entity &&
               document_->selected_sketch_entity_ids.size() == 1 &&
               dimension.entity_id == document_->selected_sketch_entity_ids.front();
      });
  document_->selected_sketch_dimension_id =
      dimension_it != feature_it->sketch_parameters->dimensions.end()
          ? std::make_optional(dimension_it->id)
          : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  return document_.value();
}
