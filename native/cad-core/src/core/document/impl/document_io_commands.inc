DocumentState DocumentManager::add_parameter(const std::string& name,
                                            const std::string& expression,
                                            const std::string& kind) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }

  // Reject duplicates
  for (const auto& p : document_->parameters) {
    if (p.name == name) {
      throw std::runtime_error("Parameter '" + name + "' already exists");
    }
  }

  push_undo_state();
  clear_redo_stack();

  ParameterEntry entry;
  entry.name = name;
  entry.expression = expression;
  entry.kind = kind;
  document_->parameters.push_back(entry);

  // Re-evaluate all parameters and refresh dimension expressions
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::update_parameter(const std::string& name,
                                                 const std::string& expression,
                                                 const std::string& kind) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }

  ParameterEntry* target = nullptr;
  for (auto& p : document_->parameters) {
    if (p.name == name) {
      target = &p;
      break;
    }
  }

  if (!target) {
    throw std::runtime_error("Parameter '" + name + "' not found");
  }

  push_undo_state();
  clear_redo_stack();

  target->expression = expression;
  target->kind = kind;

  // Re-evaluate all parameters and refresh dimension expressions
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::delete_parameter(const std::string& name) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }

  auto it = document_->parameters.begin();
  for (; it != document_->parameters.end(); ++it) {
    if (it->name == name) {
      break;
    }
  }

  if (it == document_->parameters.end()) {
    throw std::runtime_error("Parameter '" + name + "' not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->parameters.erase(it);

  // Re-evaluate remaining parameters (those referencing the deleted
  // one will now have errors) and refresh dimension expressions
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();

  return document_.value();
}

DocumentState DocumentManager::update_selection_filter(
    const SelectionFilter& filter) {
  require_document();

  push_undo_state();
  clear_redo_stack();

  document_->selection_filter = filter;

  return document_.value();
}

DocumentState DocumentManager::clear_selection() {
  require_document();

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

ExportResult DocumentManager::export_document_as_step(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_step(document_.value(), file_path);
}

ExportResult DocumentManager::export_document_as_stl(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_stl(document_.value(), file_path);
}

ExportResult DocumentManager::export_body_as_stl(
    const std::string& file_path,
    const std::string& body_id) const {
  require_document();
  return polysmith::core::export_body_as_stl(document_.value(),
                                             file_path,
                                             body_id);
}

namespace {

// Extract the trailing positive integer from id strings like "feature-12",
// "doc-3", "line-7". Returns 0 if no trailing integer is present.
int trailing_integer(const std::string& id) {
  size_t i = id.size();
  while (i > 0 && id[i - 1] >= '0' && id[i - 1] <= '9') {
    --i;
  }
  if (i == id.size()) {
    return 0;
  }
  try {
    return std::stoi(id.substr(i));
  } catch (...) {
    return 0;
  }
}

}  // namespace
