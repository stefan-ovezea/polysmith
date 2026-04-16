#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/box_feature.h"
#include "core/cylinder_feature.h"
#include "core/feature.h"
#include "core/sketch_feature.h"

namespace polysmith::core {

struct DocumentState {
  std::string id;
  std::string name;
  std::string units;
  int revision;
  std::optional<std::string> selected_feature_id;
  std::optional<std::string> selected_reference_id;
  std::optional<std::string> active_sketch_plane_id;
  std::optional<std::string> active_sketch_feature_id;
  std::optional<std::string> active_sketch_tool;
  std::optional<std::string> selected_sketch_entity_id;
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
  DocumentState add_cylinder_feature(const CylinderFeatureParameters& parameters);
  DocumentState update_box_feature(const std::string& feature_id,
                                   const BoxFeatureParameters& parameters);
  DocumentState rename_feature(const std::string& feature_id,
                               const std::string& name);
  DocumentState delete_feature(const std::string& feature_id);
  DocumentState undo();
  DocumentState redo();
  DocumentState select_feature(const std::string& feature_id);
  DocumentState select_reference(const std::string& reference_id);
  DocumentState start_sketch_on_plane(const std::string& reference_id);
  DocumentState set_sketch_tool(const std::string& tool);
  DocumentState add_sketch_line(double start_x,
                                double start_y,
                                double end_x,
                                double end_y);
  DocumentState add_sketch_rectangle(double start_x,
                                     double start_y,
                                     double end_x,
                                     double end_y);
  DocumentState add_sketch_circle(double center_x,
                                  double center_y,
                                  double radius);
  DocumentState select_sketch_entity(const std::string& entity_id);
  DocumentState finish_sketch();
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
  int next_sketch_line_id_ = 1;
  int next_sketch_circle_id_ = 1;
  int document_count_ = 0;
  std::optional<DocumentState> document_;
  std::vector<DocumentState> undo_stack_;
  std::vector<DocumentState> redo_stack_;
};

}  // namespace polysmith::core
