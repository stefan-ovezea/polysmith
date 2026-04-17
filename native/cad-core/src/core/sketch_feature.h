#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame = std::nullopt);
void set_sketch_tool(FeatureEntry& feature, const std::string& tool);
void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y);
void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint);
void set_sketch_equal_length_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_perpendicular_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_parallel_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id);
void set_sketch_coincident_constraint(
    FeatureEntry& feature,
    const std::string& point_id,
    const std::string& other_point_id);
void update_sketch_circle(FeatureEntry& feature,
                          const std::string& circle_id,
                          double center_x,
                          double center_y,
                          double radius);
void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value);
void add_sketch_line(FeatureEntry& feature,
                     int line_index,
                     double start_x,
                     double start_y,
                     double end_x,
                     double end_y);
void add_sketch_rectangle(FeatureEntry& feature,
                          int& next_line_index,
                          double start_x,
                          double start_y,
                          double end_x,
                          double end_y);
void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius);

}  // namespace polysmith::core
