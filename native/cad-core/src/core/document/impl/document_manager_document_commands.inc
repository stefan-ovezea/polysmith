  DocumentState create_document();
  DocumentState rename_feature(const std::string& feature_id,
                               const std::string& name);
  // Toggle a feature's suppressed flag. Suppressed features are
  // excluded from body compilation (see body_compiler.cpp) and legacy
  // primitive emission (viewport.cpp), but remain in feature_history
  // so they can be reactivated. Throws if the id doesn't exist or
  // refers to the root feature, which can't be suppressed.
  DocumentState set_feature_suppressed(const std::string& feature_id,
                                       bool suppressed);
  DocumentState delete_feature(const std::string& feature_id);
  DocumentState undo();
  DocumentState redo();
  DocumentState set_timeline_cursor(int included_action_count);
  void save_document_to_path(const std::string& file_path) const;
  DocumentState load_document_from_path(const std::string& file_path);
  std::optional<DocumentState> get_document() const;
  SessionState get_session_state() const;
