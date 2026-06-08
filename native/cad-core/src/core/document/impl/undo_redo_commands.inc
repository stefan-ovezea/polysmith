DocumentState DocumentManager::undo() {
  require_document();

  if (undo_stack_.empty()) {
    throw std::runtime_error("Nothing to undo");
  }

  redo_stack_.push_back(document_.value());
  document_ = undo_stack_.back();
  undo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::redo() {
  require_document();

  if (redo_stack_.empty()) {
    throw std::runtime_error("Nothing to redo");
  }

  undo_stack_.push_back(document_.value());
  document_ = redo_stack_.back();
  redo_stack_.pop_back();
  return document_.value();
}
