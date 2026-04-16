#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_sketch_feature(int feature_index, const std::string& plane_id);
void set_sketch_tool(FeatureEntry& feature, const std::string& tool);
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
