#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/box_feature.h"
#include "core/feature.h"

namespace polysmith::core {

struct DocumentState {
  std::string id;
  std::string name;
  std::string units;
  int revision;
  std::optional<std::string> selected_feature_id;
  std::vector<FeatureEntry> feature_history;
};

struct SessionState {
  int document_count;
  std::optional<std::string> active_document_id;
  bool can_undo;
  bool can_redo;
};

class DocumentManager {
 public:
  DocumentState create_document();
  DocumentState add_box_feature(const BoxFeatureParameters& parameters);
  DocumentState update_box_feature(const std::string& feature_id,
                                   const BoxFeatureParameters& parameters);
  DocumentState rename_feature(const std::string& feature_id,
                               const std::string& name);
  DocumentState delete_feature(const std::string& feature_id);
  DocumentState undo();
  DocumentState redo();
  DocumentState select_feature(const std::string& feature_id);
  DocumentState clear_selection();
  std::optional<DocumentState> get_document() const;
  SessionState get_session_state() const;

 private:
  FeatureEntry make_root_feature();
  void require_document() const;
  void push_undo_state();
  void clear_redo_stack();

  int next_document_id_ = 1;
  int next_feature_id_ = 1;
  int document_count_ = 0;
  std::optional<DocumentState> document_;
  std::vector<DocumentState> undo_stack_;
  std::vector<DocumentState> redo_stack_;
};

}  // namespace polysmith::core
