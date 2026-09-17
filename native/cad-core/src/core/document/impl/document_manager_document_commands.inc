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
  // Undo (or redo) up to `count` steps in one call, running the full
  // refresh pipeline ONCE at the end (undo_many with a negative count
  // redoes). Clamped to the available steps; inside an open undo group
  // it is clamped to the session-local steps so a grouped session can
  // never swallow pre-session history.
  DocumentState undo_many(int count);
  // Named grouped steps (D2): a user intent that spans several
  // commands (sketch session, dimension draft session, projection
  // remove) opens a group.  Pushes inside the group collapse into ONE
  // entry — the group's start snapshot — when it ends; per-action
  // undo/redo while the group is open only walks steps pushed after
  // the group began.  abort_undo_group restores the start snapshot and
  // leaves no trace (cancel semantics).  Groups nest; only the
  // outermost frame's name and snapshot are kept.
  void begin_undo_group(const std::string& name);
  void end_undo_group();
  DocumentState abort_undo_group();
  // Aborts the WHOLE group stack — nested frames included — restoring
  // the outermost frame's start snapshot.  This is the "Cancel Sketch"
  // contract (D4): the entire edit session rolls back with no trace,
  // even when a nested draft group is open.
  DocumentState abort_all_undo_groups();
  // Marks the open group as changed even when no push happened (e.g.
  // a bookkeeping sweep with no surviving entities to delete).  Rule:
  // no history entry without a state change.
  void mark_undo_group_changed();
  void set_undo_limit(int limit);
  int undo_limit() const;
  DocumentState set_timeline_cursor(int included_action_count);
  void save_document_to_path(const std::string& file_path) const;
  DocumentState load_document_from_path(const std::string& file_path);
  std::optional<DocumentState> get_document() const;
  SessionState get_session_state() const;
