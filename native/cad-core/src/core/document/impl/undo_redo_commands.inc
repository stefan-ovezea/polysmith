// Every restore path (undo/redo/undo_many/abort) runs the full
// post-restore pipeline: cam_runtime::invalidate() BEFORE the bump
// (a revision stamp alone cannot tell a path stored on the abandoned
// branch from a current one — the restored revision can collide with
// the abandoned branch's value), then bump_geometry_revision() which
// re-runs the dependency passes, normalizes the timeline cursor,
// prunes dead selection ids, and advances the revision monotonically.
// The raw swap used to leave the restored snapshot's OLDER revision
// in place, so the revision clock went backwards.

DocumentState DocumentManager::undo() {
  require_document();

  if (undo_stack_.empty()) {
    throw std::runtime_error("Nothing to undo");
  }
  // Inside an open group, undo only walks steps pushed after the group
  // began (per-action undo within the session); pre-session history is
  // reached after the group ends.
  if (!undo_group_stack_.empty() &&
      undo_stack_.size() <= undo_group_stack_.back().undo_start_index) {
    return document_.value();
  }

  const int pre_restore_revision = document_->revision;
  cam_runtime::invalidate(document_->id);
  drawing_runtime::invalidate(document_->id);
  redo_stack_.push_back(document_.value());
  redo_names_.push_back(undo_names_.back());
  document_ = undo_stack_.back();
  undo_stack_.pop_back();
  undo_names_.pop_back();

  // Snapshots carry the revision of their own era, which can lag the
  // live clock (group collapses, several bumps per step).  Floor the
  // restored revision at the pre-restore value so the bump always
  // advances monotonically.
  document_->revision = std::max(document_->revision, pre_restore_revision);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::redo() {
  require_document();

  if (redo_stack_.empty()) {
    throw std::runtime_error("Nothing to redo");
  }
  if (!undo_group_stack_.empty() &&
      redo_stack_.size() <= undo_group_stack_.back().redo_start_index) {
    return document_.value();
  }

  const int pre_restore_revision = document_->revision;
  cam_runtime::invalidate(document_->id);
  drawing_runtime::invalidate(document_->id);
  undo_stack_.push_back(document_.value());
  undo_names_.push_back(redo_names_.back());
  document_ = redo_stack_.back();
  redo_stack_.pop_back();
  redo_names_.pop_back();

  document_->revision = std::max(document_->revision, pre_restore_revision);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::undo_many(int count) {
  require_document();

  const int pre_restore_revision = document_->revision;
  int steps = count;
  // Negative count = redo direction.
  if (steps < 0) {
    steps = -steps;
    if (!undo_group_stack_.empty()) {
      const size_t local =
          redo_stack_.size() - undo_group_stack_.back().redo_start_index;
      steps = std::min(steps, static_cast<int>(local));
    }
    steps = std::min(steps, static_cast<int>(redo_stack_.size()));
    if (steps > 0) {
      cam_runtime::invalidate(document_->id);
      drawing_runtime::invalidate(document_->id);
    }
    for (int i = 0; i < steps; ++i) {
      undo_stack_.push_back(document_.value());
      undo_names_.push_back(redo_names_.back());
      document_ = redo_stack_.back();
      redo_stack_.pop_back();
      redo_names_.pop_back();
    }
  } else {
    if (!undo_group_stack_.empty()) {
      const size_t local =
          undo_stack_.size() - undo_group_stack_.back().undo_start_index;
      steps = std::min(steps, static_cast<int>(local));
    }
    steps = std::min(steps, static_cast<int>(undo_stack_.size()));
    if (steps > 0) {
      cam_runtime::invalidate(document_->id);
      drawing_runtime::invalidate(document_->id);
    }
    for (int i = 0; i < steps; ++i) {
      redo_stack_.push_back(document_.value());
      redo_names_.push_back(undo_names_.back());
      document_ = undo_stack_.back();
      undo_stack_.pop_back();
      undo_names_.pop_back();
    }
  }

  // ONE refresh for the whole batch (the history dropdown semantics).
  document_->revision = std::max(document_->revision, pre_restore_revision);
  bump_geometry_revision();
  return document_.value();
}

void DocumentManager::set_undo_limit(int limit) {
  max_undo_steps_ = std::max(1, limit);
  while (undo_stack_.size() > static_cast<size_t>(max_undo_steps_)) {
    undo_stack_.erase(undo_stack_.begin());
    undo_names_.erase(undo_names_.begin());
  }
  sync_undo_step_names();
}

int DocumentManager::undo_limit() const {
  return max_undo_steps_;
}
