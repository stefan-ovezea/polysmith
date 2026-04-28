#pragma once

#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

struct SketchProfilePoint {
  double x;
  double y;
};

struct BoxFeatureParameters {
  double width;
  double height;
  double depth;
};

struct CylinderFeatureParameters {
  double radius;
  double height;
};

struct PlaneFrame {
  double origin_x;
  double origin_y;
  double origin_z;
  double x_axis_x;
  double x_axis_y;
  double x_axis_z;
  double y_axis_x;
  double y_axis_y;
  double y_axis_z;
  double normal_x;
  double normal_y;
  double normal_z;
};

struct ExtrudeFeatureParameters {
  std::string sketch_feature_id;
  std::string profile_id;
  std::string plane_id;
  std::optional<PlaneFrame> plane_frame;
  std::string profile_kind;
  double start_x;
  double start_y;
  double width;
  double height;
  double radius;
  std::vector<SketchProfilePoint> profile_points;
  double depth;
};

struct SketchLine {
  std::string id;
  std::string start_point_id;
  std::string end_point_id;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  std::optional<std::string> constraint;
};

struct SketchCircle {
  std::string id;
  double center_x;
  double center_y;
  double radius;
};

struct SketchPoint {
  std::string id;
  std::string kind;
  double x;
  double y;
  bool is_fixed;
};

struct SketchDimension {
  std::string id;
  std::string kind;
  std::string entity_id;
  double value;
};

struct SketchLineRelation {
  std::string id;
  std::string kind;
  std::string first_line_id;
  std::string second_line_id;
};

struct SketchProfileRegion {
  std::string id;
  std::string kind;
  std::vector<std::string> point_ids;
  std::vector<std::string> line_ids;
  std::vector<SketchProfilePoint> points;
  std::optional<std::string> source_circle_id;
  double center_x;
  double center_y;
  double radius;
};

struct SketchFeatureParameters {
  struct SketchPlaneFrame {
    double origin_x;
    double origin_y;
    double origin_z;
    double x_axis_x;
    double x_axis_y;
    double x_axis_z;
    double y_axis_x;
    double y_axis_y;
    double y_axis_z;
    double normal_x;
    double normal_y;
    double normal_z;
  };

  std::string plane_id;
  std::optional<SketchPlaneFrame> plane_frame;
  std::string active_tool;
  std::vector<SketchLine> lines;
  std::vector<SketchCircle> circles;
  std::vector<SketchPoint> points;
  std::vector<SketchDimension> dimensions;
  std::vector<SketchLineRelation> line_relations;
  std::vector<SketchProfileRegion> profiles;
};

struct FeatureEntry {
  std::string id;
  std::string kind;
  std::string name;
  std::string status;
  std::string parameters_summary;
  std::optional<BoxFeatureParameters> box_parameters;
  std::optional<CylinderFeatureParameters> cylinder_parameters;
  std::optional<ExtrudeFeatureParameters> extrude_parameters;
  std::optional<SketchFeatureParameters> sketch_parameters;
};

}  // namespace polysmith::core
