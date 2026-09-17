// Named grouped undo steps (D2).  A user intent that spans several
// commands — a sketch edit session, a dimension draft session, a
// projection remove that reuses delete_sketch_selection — opens a
// group around the sequence.  While a group is open:
//   * push_undo_state() still pushes onto the main stack, ABOVE the
//     group's marker, so undo()/redo() inside the session walk only
//     the session-local steps (per-action granularity while the
//     sketch/dialog is open — the Onshape model).
//   * end_undo_group() collapses everything above the marker into ONE
//     entry: the group's start snapshot, carrying the group's name.
//     No entry appears when nothing changed (no history entry without
//     a state change).
//   * abort_undo_group() restores the start snapshot, drops every
//     session-local step, and leaves no trace (cancel semantics).
// Groups nest; the outermost frame owns the name and snapshot.

void DocumentManager::begin_undo_group(const std::string& name) {
  require_document();
  undo_group_stack_.push_back(UndoGroupFrame{
      .name = name,
      .start_snapshot = document_.value(),
      .undo_start_index = undo_stack_.size(),
      .redo_start_index = redo_stack_.size(),
      .changed = false,
  });
}

void DocumentManager::mark_undo_group_changed() {
  if (!undo_group_stack_.empty()) {
    undo_group_stack_.back().changed = true;
  }
}

void DocumentManager::end_undo_group() {
  require_document();
  if (undo_group_stack_.empty()) {
    return;
  }
  const UndoGroupFrame frame = undo_group_stack_.back();
  undo_group_stack_.pop_back();

  // The frame's changed flag is the "did any mutating command run"
  // signal (set by push_undo_state / mark_undo_group_changed).  It is
  // deliberately NOT a revision or stack-size comparison: undoing a
  // step inside the session moves the revision without a net state
  // change, and a pure view mutation (set_sketch_tool) must never
  // leave an entry.
  const bool changed = frame.changed;

  // Collapse the session steps into the single group entry.
  undo_stack_.resize(frame.undo_start_index);
  undo_names_.resize(frame.undo_start_index);
  if (changed) {
    undo_stack_.push_back(frame.start_snapshot);
    undo_names_.push_back(frame.name);
    while (undo_stack_.size() > static_cast<size_t>(max_undo_steps_)) {
      undo_stack_.erase(undo_stack_.begin());
      undo_names_.erase(undo_names_.begin());
    }
    // A collapsed group is itself one step — an enclosing group must
    // fold it in as well.
    if (!undo_group_stack_.empty()) {
      undo_group_stack_.back().changed = true;
    }
  }
  clear_redo_stack();
}

DocumentState DocumentManager::abort_undo_group() {
  require_document();
  if (undo_group_stack_.empty()) {
    throw std::runtime_error("No active undo group");
  }
  const UndoGroupFrame frame = undo_group_stack_.back();
  undo_group_stack_.pop_back();

  // Restore the pre-session snapshot and drop every session-local step
  // on both stacks — the session leaves no trace (cancel semantics).
  const int pre_restore_revision = document_->revision;
  document_ = frame.start_snapshot;
  undo_stack_.resize(frame.undo_start_index);
  undo_names_.resize(frame.undo_start_index);
  redo_stack_.resize(frame.redo_start_index);
  redo_names_.resize(frame.redo_start_index);

  document_->revision = std::max(document_->revision, pre_restore_revision);
  cam_runtime::invalidate(document_->id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::abort_all_undo_groups() {
  require_document();
  if (undo_group_stack_.empty()) {
    throw std::runtime_error("No active undo group");
  }

  // Cancel-the-whole-session semantics (D4 "Cancel Sketch"): the
  // OUTERMOST frame is the session itself, so nested frames (dimension
  // drafts, projection removes) roll back with it.  Restore the
  // session's start snapshot and leave no trace on either stack.
  const UndoGroupFrame frame = undo_group_stack_.front();
  undo_group_stack_.clear();

  const int pre_restore_revision = document_->revision;
  document_ = frame.start_snapshot;
  undo_stack_.resize(frame.undo_start_index);
  undo_names_.resize(frame.undo_start_index);
  redo_stack_.resize(frame.redo_start_index);
  redo_names_.resize(frame.redo_start_index);

  document_->revision = std::max(document_->revision, pre_restore_revision);
  cam_runtime::invalidate(document_->id);
  bump_geometry_revision();
  return document_.value();
}
