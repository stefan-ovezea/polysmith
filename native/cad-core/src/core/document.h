#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/box_feature.h"
#include "core/cylinder_feature.h"
#include "core/export.h"
#include "core/extrude_feature.h"
#include "core/feature.h"
#include "core/sketch_profile.h"
#include "core/sketch_feature.h"

namespace polysmith::core {

struct DocumentState {
  std::string id;
  std::string name;
  std::string units;
  int revision;
  std::optional<std::string> selected_feature_id;
  std::optional<std::string> selected_reference_id;
  std::optional<std::string> selected_face_id;
  std::optional<std::string> active_sketch_plane_id;
  std::optional<std::string> active_sketch_face_id;
  std::optional<std::string> active_sketch_feature_id;
  std::optional<std::string> active_sketch_tool;
  std::optional<std::string> selected_sketch_point_id;
  std::optional<std::string> selected_sketch_entity_id;
  std::optional<std::string> selected_sketch_dimension_id;
  std::optional<std::string> selected_sketch_profile_id;
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
  DocumentState update_extrude_depth(const std::string& feature_id,
                                     double depth);
  DocumentState rename_feature(const std::string& feature_id,
                               const std::string& name);
  DocumentState delete_feature(const std::string& feature_id);
  DocumentState undo();
  DocumentState redo();
  DocumentState select_feature(const std::string& feature_id);
  DocumentState select_reference(const std::string& reference_id);
  DocumentState select_face(const std::string& face_id);
  DocumentState start_sketch_on_plane(const std::string& reference_id);
  DocumentState start_sketch_on_face(
      const std::string& face_id,
      const SketchFeatureParameters::SketchPlaneFrame& plane_frame);
  DocumentState set_sketch_tool(const std::string& tool);
  DocumentState update_sketch_line(const std::string& line_id,
                                   double start_x,
                                   double start_y,
                                   double end_x,
                                   double end_y);
  DocumentState update_sketch_point(const std::string& point_id,
                                    double x,
                                    double y);
  DocumentState set_sketch_line_constraint(
      const std::string& line_id,
      const std::optional<std::string>& constraint);
  DocumentState set_sketch_equal_length_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState set_sketch_perpendicular_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState set_sketch_parallel_constraint(
      const std::string& line_id,
      const std::optional<std::string>& other_line_id);
  DocumentState set_sketch_coincident_constraint(const std::string& point_id,
                                                 const std::string& other_point_id);
  DocumentState set_sketch_point_fixed(const std::string& point_id,
                                       bool is_fixed);
  DocumentState update_sketch_circle(const std::string& circle_id,
                                     double center_x,
                                     double center_y,
                                     double radius);
  DocumentState update_sketch_dimension(const std::string& dimension_id,
                                        double value);
  DocumentState select_sketch_profile(const std::string& profile_id);
  DocumentState extrude_profile(const std::string& profile_id, double depth);
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
  DocumentState select_sketch_point(const std::string& point_id);
  DocumentState select_sketch_entity(const std::string& entity_id);
  DocumentState select_sketch_dimension(const std::string& dimension_id);
  DocumentState finish_sketch();
  DocumentState reenter_sketch(const std::string& feature_id);
  DocumentState clear_selection();
  ExportResult export_document_as_step(const std::string& file_path) const;
  ExportResult export_document_as_stl(const std::string& file_path) const;
  void save_document_to_path(const std::string& file_path) const;
  DocumentState load_document_from_path(const std::string& file_path);
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
