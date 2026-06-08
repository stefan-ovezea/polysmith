// Tiny helper used by all five mirror_preview wrappers — they
// all need to find the active sketch feature and bail out if
// there isn't one. Returns a non-const iterator into
// `feature_history` so the caller can mutate it.
namespace {
std::vector<FeatureEntry>::iterator require_active_sketch(
    DocumentState& document) {
  if (!document.active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto& sketch_id = *document.active_sketch_feature_id;
  const auto feature_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == sketch_id; });
  if (feature_it == document.feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  return feature_it;
}
}  // namespace

DocumentState DocumentManager::start_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  // No undo push — preview state is transient and we don't want
  // it to clutter the undo history.
  polysmith::core::start_mirror_preview(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_mirror_preview_axis(
    const std::string& axis_line_id) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::update_mirror_preview_axis(*feature_it, axis_line_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_mirror_preview_objects(
    const std::vector<std::string>& object_ids) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::update_mirror_preview_objects(*feature_it, object_ids);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::commit_mirror_preview(bool persistent) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  // Commit is the only mirror_preview op that actually changes
  // permanent geometry, so it's the one that pushes an undo
  // state. Cancel/start/update don't write anything that needs
  // rolling back beyond the in-memory pending struct.
  push_undo_state();
  clear_redo_stack();
  polysmith::core::commit_mirror_preview(
      *feature_it, next_sketch_line_id_, next_sketch_circle_id_, persistent);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cancel_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::cancel_mirror_preview(*feature_it);
  bump_geometry_revision();
  return document_.value();
}
