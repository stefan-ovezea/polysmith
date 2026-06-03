#include "protocol/serialization.h"

#include <cmath>
#include <stdexcept>

#include "core/dof_counter.h"

namespace polysmith::protocol {

namespace {

const json& require(const json& payload, const char* key) {
  if (!payload.is_object() || !payload.contains(key)) {
    throw std::runtime_error(std::string("Missing field: ") + key);
  }
  return payload.at(key);
}

std::string read_string(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_string()) {
    throw std::runtime_error(std::string("Field is not a string: ") + key);
  }
  return value.get<std::string>();
}

double read_number(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_number()) {
    throw std::runtime_error(std::string("Field is not a number: ") + key);
  }
  return value.get<double>();
}

int read_int(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_number_integer() && !value.is_number()) {
    throw std::runtime_error(std::string("Field is not an integer: ") + key);
  }
  return value.get<int>();
}

bool read_bool(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_boolean()) {
    throw std::runtime_error(std::string("Field is not a bool: ") + key);
  }
  return value.get<bool>();
}

std::optional<std::string> read_optional_string(const json& payload,
                                                const char* key) {
  if (!payload.is_object() || !payload.contains(key) || payload.at(key).is_null()) {
    return std::nullopt;
  }
  return payload.at(key).get<std::string>();
}

double read_optional_number(const json& payload,
                            const char* key,
                            double fallback) {
  if (!payload.is_object() || !payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Field is not a number: ") + key);
  }
  return payload.at(key).get<double>();
}

std::string read_optional_string_value(const json& payload,
                                       const char* key,
                                       const std::string& fallback) {
  if (!payload.is_object() || !payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Field is not a string: ") + key);
  }
  return payload.at(key).get<std::string>();
}

polysmith::core::PlaneFrame plane_frame_from_payload(const json& payload) {
  polysmith::core::PlaneFrame frame{};
  frame.origin_x = require(payload, "origin").at("x").get<double>();
  frame.origin_y = require(payload, "origin").at("y").get<double>();
  frame.origin_z = require(payload, "origin").at("z").get<double>();
  frame.x_axis_x = require(payload, "x_axis").at("x").get<double>();
  frame.x_axis_y = require(payload, "x_axis").at("y").get<double>();
  frame.x_axis_z = require(payload, "x_axis").at("z").get<double>();
  frame.y_axis_x = require(payload, "y_axis").at("x").get<double>();
  frame.y_axis_y = require(payload, "y_axis").at("y").get<double>();
  frame.y_axis_z = require(payload, "y_axis").at("z").get<double>();
  frame.normal_x = require(payload, "normal").at("x").get<double>();
  frame.normal_y = require(payload, "normal").at("y").get<double>();
  frame.normal_z = require(payload, "normal").at("z").get<double>();
  return frame;
}

polysmith::core::ExtrudeFeatureParameters::SideParameters
extrude_side_from_payload(
    const json& payload,
    const polysmith::core::ExtrudeFeatureParameters::SideParameters& fallback) {
  polysmith::core::ExtrudeFeatureParameters::SideParameters side = fallback;
  if (!payload.is_object()) {
    return side;
  }
  side.extent_type =
      read_optional_string_value(payload, "extent_type", side.extent_type);
  side.distance = read_optional_number(payload, "distance", side.distance);
  side.start_offset =
      read_optional_number(payload, "start_offset", side.start_offset);
  side.taper_angle_degrees =
      read_optional_number(payload,
                           "taper_angle_degrees",
                           side.taper_angle_degrees);
  side.target_reference_id = read_optional_string(payload, "target_reference_id");
  return side;
}

json extrude_side_to_payload(
    const polysmith::core::ExtrudeFeatureParameters::SideParameters& side) {
  return json{
      {"extent_type", side.extent_type},
      {"distance", side.distance},
      {"start_offset", side.start_offset},
      {"taper_angle_degrees", side.taper_angle_degrees},
      {"target_reference_id",
       side.target_reference_id.has_value()
           ? json(side.target_reference_id.value())
           : json(nullptr)},
  };
}

polysmith::core::SketchFeatureParameters::SketchPlaneFrame
sketch_plane_frame_from_payload(const json& payload) {
  polysmith::core::SketchFeatureParameters::SketchPlaneFrame frame{};
  const polysmith::core::PlaneFrame base = plane_frame_from_payload(payload);
  frame.origin_x = base.origin_x;
  frame.origin_y = base.origin_y;
  frame.origin_z = base.origin_z;
  frame.x_axis_x = base.x_axis_x;
  frame.x_axis_y = base.x_axis_y;
  frame.x_axis_z = base.x_axis_z;
  frame.y_axis_x = base.y_axis_x;
  frame.y_axis_y = base.y_axis_y;
  frame.y_axis_z = base.y_axis_z;
  frame.normal_x = base.normal_x;
  frame.normal_y = base.normal_y;
  frame.normal_z = base.normal_z;
  return frame;
}

json viewport_sketch_plane_frame_to_payload(
    const polysmith::core::ViewportSketchPlaneFrame& frame) {
  return {
      {"origin",
       {
           {"x", frame.origin_x},
           {"y", frame.origin_y},
           {"z", frame.origin_z},
       }},
      {"x_axis",
       {
           {"x", frame.x_axis_x},
           {"y", frame.x_axis_y},
           {"z", frame.x_axis_z},
       }},
      {"y_axis",
       {
           {"x", frame.y_axis_x},
           {"y", frame.y_axis_y},
           {"z", frame.y_axis_z},
       }},
      {"normal",
       {
           {"x", frame.normal_x},
           {"y", frame.normal_y},
           {"z", frame.normal_z},
       }},
  };
}

polysmith::core::ExtrudeFeatureParameters
parse_extrude_parameters_from_payload(const json& payload) {
  polysmith::core::ExtrudeFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  if (payload.contains("profile_ids") && payload.at("profile_ids").is_array()) {
    for (const auto& id_value : payload.at("profile_ids")) {
      params.profile_ids.push_back(id_value.get<std::string>());
    }
  } else if (!params.profile_id.empty()) {
    params.profile_ids.push_back(params.profile_id);
  }
  if (payload.contains("open_entity_ids") &&
      payload.at("open_entity_ids").is_array()) {
    for (const auto& id_value : payload.at("open_entity_ids")) {
      params.open_entity_ids.push_back(id_value.get<std::string>());
    }
  }
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.profile_kind = read_string(payload, "profile_kind");
  params.start_x = read_number(payload, "start_x");
  params.start_y = read_number(payload, "start_y");
  params.width = read_number(payload, "width");
  params.height = read_number(payload, "height");
  params.radius = read_number(payload, "radius");
  if (payload.contains("profile_points") && payload.at("profile_points").is_array()) {
    for (const auto& point_payload : payload.at("profile_points")) {
      params.profile_points.push_back(polysmith::core::SketchProfilePoint{
          .x = point_payload.at("x").get<double>(),
          .y = point_payload.at("y").get<double>(),
      });
    }
  }
  if (payload.contains("inner_loops") && payload.at("inner_loops").is_array()) {
    for (const auto& loop_payload : payload.at("inner_loops")) {
      std::vector<polysmith::core::SketchProfilePoint> loop;
      for (const auto& point_payload : loop_payload) {
        loop.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.inner_loops.push_back(std::move(loop));
    }
  }
  if (payload.contains("additional_profile_points") &&
      payload.at("additional_profile_points").is_array()) {
    for (const auto& profile_payload : payload.at("additional_profile_points")) {
      std::vector<polysmith::core::SketchProfilePoint> profile;
      for (const auto& point_payload : profile_payload) {
        profile.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.additional_profile_points.push_back(std::move(profile));
    }
  }
  if (payload.contains("additional_inner_loops") &&
      payload.at("additional_inner_loops").is_array()) {
    for (const auto& profile_payload : payload.at("additional_inner_loops")) {
      std::vector<std::vector<polysmith::core::SketchProfilePoint>> loops;
      for (const auto& loop_payload : profile_payload) {
        std::vector<polysmith::core::SketchProfilePoint> loop;
        for (const auto& point_payload : loop_payload) {
          loop.push_back(polysmith::core::SketchProfilePoint{
              .x = point_payload.at("x").get<double>(),
              .y = point_payload.at("y").get<double>(),
          });
        }
        loops.push_back(std::move(loop));
      }
      params.additional_inner_loops.push_back(std::move(loops));
    }
  }
  params.depth = read_number(payload, "depth");
  params.extent_mode =
      read_optional_string_value(payload, "extent_mode", params.extent_mode);
  params.side1.distance = std::abs(params.depth);
  if (payload.contains("side1") && payload.at("side1").is_object()) {
    params.side1 = extrude_side_from_payload(payload.at("side1"), params.side1);
  }
  if (payload.contains("side2") && payload.at("side2").is_object()) {
    params.side2 =
        extrude_side_from_payload(payload.at("side2"), params.side1);
  }
  if (payload.contains("thin") && payload.at("thin").is_object()) {
    const auto& thin = payload.at("thin");
    if (thin.contains("enabled") && thin.at("enabled").is_boolean()) {
      params.thin.enabled = thin.at("enabled").get<bool>();
    }
    params.thin.thickness =
        read_optional_number(thin, "thickness", params.thin.thickness);
    params.thin.placement =
        read_optional_string_value(thin, "placement", params.thin.placement);
  }
  if (payload.contains("mode") && payload.at("mode").is_string()) {
    params.mode = payload.at("mode").get<std::string>();
  }
  params.operation =
      read_optional_string_value(payload, "operation", params.mode);
  params.intersect_result =
      read_optional_string_value(payload,
                                 "intersect_result",
                                 params.intersect_result);
  if (payload.contains("target_body_id") &&
      payload.at("target_body_id").is_string()) {
    params.target_body_id = payload.at("target_body_id").get<std::string>();
  }
  return params;
}

polysmith::core::LoftFeatureParameters
loft_parameters_from_payload(const json& payload) {
  polysmith::core::LoftFeatureParameters params{};
  if (payload.contains("ruled") && payload.at("ruled").is_boolean()) {
    params.ruled = payload.at("ruled").get<bool>();
  }
  if (payload.contains("sections") && payload.at("sections").is_array()) {
    for (const auto& section_payload : payload.at("sections")) {
      polysmith::core::LoftSectionParameters section{};
      section.sketch_feature_id =
          read_string(section_payload, "sketch_feature_id");
      section.profile_id = read_string(section_payload, "profile_id");
      section.plane_id = read_string(section_payload, "plane_id");
      if (section_payload.contains("plane_frame") &&
          !section_payload.at("plane_frame").is_null()) {
        section.plane_frame =
            plane_frame_from_payload(section_payload.at("plane_frame"));
      }
      if (section_payload.contains("profile_points") &&
          section_payload.at("profile_points").is_array()) {
        for (const auto& point_payload : section_payload.at("profile_points")) {
          section.profile_points.push_back(polysmith::core::SketchProfilePoint{
              .x = point_payload.at("x").get<double>(),
              .y = point_payload.at("y").get<double>(),
          });
        }
      }
      params.sections.push_back(std::move(section));
    }
  }
  return params;
}

polysmith::core::RevolveFeatureParameters
revolve_parameters_from_payload(const json& payload) {
  polysmith::core::RevolveFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.profile_kind = read_string(payload, "profile_kind");
  if (payload.contains("profile_points") &&
      payload.at("profile_points").is_array()) {
    for (const auto& point_payload : payload.at("profile_points")) {
      params.profile_points.push_back(polysmith::core::SketchProfilePoint{
          .x = point_payload.at("x").get<double>(),
          .y = point_payload.at("y").get<double>(),
      });
    }
  }
  if (payload.contains("inner_loops") && payload.at("inner_loops").is_array()) {
    for (const auto& loop_payload : payload.at("inner_loops")) {
      std::vector<polysmith::core::SketchProfilePoint> loop;
      for (const auto& point_payload : loop_payload) {
        loop.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.inner_loops.push_back(std::move(loop));
    }
  }
  params.axis_sketch_feature_id =
      read_string(payload, "axis_sketch_feature_id");
  params.axis_entity_id = read_string(payload, "axis_entity_id");
  params.axis_start_x = read_number(payload, "axis_start_x");
  params.axis_start_y = read_number(payload, "axis_start_y");
  params.axis_start_z = read_number(payload, "axis_start_z");
  params.axis_end_x = read_number(payload, "axis_end_x");
  params.axis_end_y = read_number(payload, "axis_end_y");
  params.axis_end_z = read_number(payload, "axis_end_z");
  params.angle_degrees = read_number(payload, "angle_degrees");
  return params;
}

polysmith::core::SweepFeatureParameters
sweep_parameters_from_payload(const json& payload) {
  polysmith::core::SweepFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.profile_kind = read_string(payload, "profile_kind");
  if (payload.contains("profile_points") &&
      payload.at("profile_points").is_array()) {
    for (const auto& point_payload : payload.at("profile_points")) {
      params.profile_points.push_back(polysmith::core::SketchProfilePoint{
          .x = point_payload.at("x").get<double>(),
          .y = point_payload.at("y").get<double>(),
      });
    }
  }
  if (payload.contains("inner_loops") && payload.at("inner_loops").is_array()) {
    for (const auto& loop_payload : payload.at("inner_loops")) {
      std::vector<polysmith::core::SketchProfilePoint> loop;
      for (const auto& point_payload : loop_payload) {
        loop.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.inner_loops.push_back(std::move(loop));
    }
  }
  params.path_sketch_feature_id =
      read_string(payload, "path_sketch_feature_id");
  params.path_entity_id = read_string(payload, "path_entity_id");
  params.path_start_x = read_number(payload, "path_start_x");
  params.path_start_y = read_number(payload, "path_start_y");
  params.path_start_z = read_number(payload, "path_start_z");
  params.path_end_x = read_number(payload, "path_end_x");
  params.path_end_y = read_number(payload, "path_end_y");
  params.path_end_z = read_number(payload, "path_end_z");
  if (payload.contains("path_segments") &&
      payload.at("path_segments").is_array()) {
    for (const auto& segment_payload : payload.at("path_segments")) {
      polysmith::core::SweepFeatureParameters::PathSegment segment{};
      segment.entity_id = read_string(segment_payload, "entity_id");
      segment.kind = read_string(segment_payload, "kind");
      segment.start_x = read_number(segment_payload, "start_x");
      segment.start_y = read_number(segment_payload, "start_y");
      segment.start_z = read_number(segment_payload, "start_z");
      segment.end_x = read_number(segment_payload, "end_x");
      segment.end_y = read_number(segment_payload, "end_y");
      segment.end_z = read_number(segment_payload, "end_z");
      segment.center_x = read_number(segment_payload, "center_x");
      segment.center_y = read_number(segment_payload, "center_y");
      segment.center_z = read_number(segment_payload, "center_z");
      segment.mid_x = read_number(segment_payload, "mid_x");
      segment.mid_y = read_number(segment_payload, "mid_y");
      segment.mid_z = read_number(segment_payload, "mid_z");
      segment.radius = read_number(segment_payload, "radius");
      segment.ccw = segment_payload.contains("ccw") &&
                    segment_payload.at("ccw").is_boolean()
                        ? segment_payload.at("ccw").get<bool>()
                        : true;
      params.path_segments.push_back(segment);
    }
  }
  if (params.path_segments.empty()) {
    params.path_segments.push_back(
        polysmith::core::SweepFeatureParameters::PathSegment{
            .entity_id = params.path_entity_id,
            .kind = "line",
            .start_x = params.path_start_x,
            .start_y = params.path_start_y,
            .start_z = params.path_start_z,
            .end_x = params.path_end_x,
            .end_y = params.path_end_y,
            .end_z = params.path_end_z,
        });
  }
  return params;
}

polysmith::core::SketchFeatureParameters
sketch_parameters_from_payload(const json& payload) {
  polysmith::core::SketchFeatureParameters params{};
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = sketch_plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.active_tool = payload.contains("active_tool") &&
                               payload.at("active_tool").is_string()
                           ? payload.at("active_tool").get<std::string>()
                           : std::string{};
  if (payload.contains("lines") && payload.at("lines").is_array()) {
    for (const auto& line_payload : payload.at("lines")) {
      polysmith::core::SketchLine line{};
      line.id = read_string(line_payload, "line_id");
      line.start_point_id = read_string(line_payload, "start_point_id");
      line.end_point_id = read_string(line_payload, "end_point_id");
      line.start_x = read_number(line_payload, "start_x");
      line.start_y = read_number(line_payload, "start_y");
      line.end_x = read_number(line_payload, "end_x");
      line.end_y = read_number(line_payload, "end_y");
      line.constraint = read_optional_string(line_payload, "constraint");
      // Older saves predate construction lines; default to solid.
      if (line_payload.contains("is_construction") &&
          line_payload.at("is_construction").is_boolean()) {
        line.is_construction = line_payload.at("is_construction").get<bool>();
      }
      params.lines.push_back(line);
    }
  }
  if (payload.contains("circles") && payload.at("circles").is_array()) {
    for (const auto& circle_payload : payload.at("circles")) {
      polysmith::core::SketchCircle circle{};
      circle.id = read_string(circle_payload, "circle_id");
      circle.center_x = read_number(circle_payload, "center_x");
      circle.center_y = read_number(circle_payload, "center_y");
      circle.radius = read_number(circle_payload, "radius");
      if (circle_payload.contains("is_construction") &&
          circle_payload.at("is_construction").is_boolean()) {
        circle.is_construction =
            circle_payload.at("is_construction").get<bool>();
      }
      params.circles.push_back(circle);
    }
  }
  // Older saves predate arcs; absence of the key just means no arcs
  // exist, mirroring how `circles` and `lines` degrade gracefully.
  if (payload.contains("arcs") && payload.at("arcs").is_array()) {
    for (const auto& arc_payload : payload.at("arcs")) {
      polysmith::core::SketchArc arc{};
      arc.id = read_string(arc_payload, "arc_id");
      arc.start_point_id = read_string(arc_payload, "start_point_id");
      arc.end_point_id = read_string(arc_payload, "end_point_id");
      arc.center_x = read_number(arc_payload, "center_x");
      arc.center_y = read_number(arc_payload, "center_y");
      arc.radius = read_number(arc_payload, "radius");
      arc.start_x = read_number(arc_payload, "start_x");
      arc.start_y = read_number(arc_payload, "start_y");
      arc.end_x = read_number(arc_payload, "end_x");
      arc.end_y = read_number(arc_payload, "end_y");
      arc.ccw = read_bool(arc_payload, "ccw");
      if (arc_payload.contains("is_construction") &&
          arc_payload.at("is_construction").is_boolean()) {
        arc.is_construction = arc_payload.at("is_construction").get<bool>();
      }
      params.arcs.push_back(arc);
    }
  }
  // Older saves predate fillets; absence of the key just means no
  // fillets exist (lines/arcs that originated as fillet outputs are
  // already round-tripped above as plain entities).
  if (payload.contains("fillets") && payload.at("fillets").is_array()) {
    for (const auto& fillet_payload : payload.at("fillets")) {
      polysmith::core::SketchFillet fillet{};
      fillet.id = read_string(fillet_payload, "fillet_id");
      fillet.corner_point_id = read_string(fillet_payload, "corner_point_id");
      fillet.corner_x = read_number(fillet_payload, "corner_x");
      fillet.corner_y = read_number(fillet_payload, "corner_y");
      fillet.line_a_id = read_string(fillet_payload, "line_a_id");
      fillet.line_b_id = read_string(fillet_payload, "line_b_id");
      fillet.trim_a_point_id = read_string(fillet_payload, "trim_a_point_id");
      fillet.trim_b_point_id = read_string(fillet_payload, "trim_b_point_id");
      fillet.arc_id = read_string(fillet_payload, "arc_id");
      fillet.radius = read_number(fillet_payload, "radius");
      params.fillets.push_back(fillet);
    }
  }
  // Older saves predate polygons; absence of the key just means none exist.
  if (payload.contains("polygons") && payload.at("polygons").is_array()) {
    for (const auto& polygon_payload : payload.at("polygons")) {
      polysmith::core::SketchPolygon polygon{};
      polygon.id = read_string(polygon_payload, "polygon_id");
      polygon.center_x = read_number(polygon_payload, "center_x");
      polygon.center_y = read_number(polygon_payload, "center_y");
      polygon.radius = read_number(polygon_payload, "radius");
      polygon.sides = static_cast<int>(read_number(polygon_payload, "sides"));
      polygon.mode = read_string(polygon_payload, "mode");
      polygon.start_x = read_number(polygon_payload, "start_x");
      polygon.start_y = read_number(polygon_payload, "start_y");
      polygon.end_x = read_number(polygon_payload, "end_x");
      polygon.end_y = read_number(polygon_payload, "end_y");
      if (polygon_payload.contains("is_construction") &&
          polygon_payload.at("is_construction").is_boolean()) {
        polygon.is_construction =
            polygon_payload.at("is_construction").get<bool>();
      }
      params.polygons.push_back(polygon);
    }
  }
  if (payload.contains("points") && payload.at("points").is_array()) {
    for (const auto& point_payload : payload.at("points")) {
      polysmith::core::SketchPoint point{};
      point.id = read_string(point_payload, "point_id");
      point.kind = read_string(point_payload, "kind");
      point.x = read_number(point_payload, "x");
      point.y = read_number(point_payload, "y");
      point.is_fixed = read_bool(point_payload, "is_fixed");
      params.points.push_back(point);
    }
  }
  if (payload.contains("dimensions") && payload.at("dimensions").is_array()) {
    for (const auto& dim_payload : payload.at("dimensions")) {
      polysmith::core::SketchDimension dimension{};
      dimension.id = read_string(dim_payload, "dimension_id");
      dimension.kind = read_string(dim_payload, "kind");
      dimension.entity_id = read_string(dim_payload, "entity_id");
      // Older saves predate angle dimensions; fall back to "" so the
      // unary dimensions round-trip without churn.
      if (dim_payload.contains("secondary_entity_id") &&
          dim_payload.at("secondary_entity_id").is_string()) {
        dimension.secondary_entity_id =
            dim_payload.at("secondary_entity_id").get<std::string>();
      }
      dimension.value = read_number(dim_payload, "value");
      // Expression field — absent in older saves, default to ""
      if (dim_payload.contains("expression") &&
          dim_payload.at("expression").is_string()) {
        dimension.expression =
            dim_payload.at("expression").get<std::string>();
      }
      // Driven field — absent in older saves, default to false (driving)
      if (dim_payload.contains("driven") && dim_payload.at("driven").is_boolean()) {
        dimension.driven = dim_payload.at("driven").get<bool>();
      }
      // Display_as field — absent in older saves, default to "" (diameter)
      if (dim_payload.contains("display_as") &&
          dim_payload.at("display_as").is_string()) {
        dimension.display_as =
            dim_payload.at("display_as").get<std::string>();
      }
      if (dim_payload.contains("label_x") &&
          dim_payload.at("label_x").is_number()) {
        dimension.label_x = dim_payload.at("label_x").get<double>();
      }
      if (dim_payload.contains("label_y") &&
          dim_payload.at("label_y").is_number()) {
        dimension.label_y = dim_payload.at("label_y").get<double>();
      }
      params.dimensions.push_back(dimension);
    }
  }
  if (payload.contains("line_relations") && payload.at("line_relations").is_array()) {
    for (const auto& relation_payload : payload.at("line_relations")) {
      polysmith::core::SketchLineRelation relation{};
      relation.id = read_string(relation_payload, "relation_id");
      relation.kind = read_string(relation_payload, "kind");
      relation.first_line_id = read_string(relation_payload, "first_line_id");
      relation.second_line_id = read_string(relation_payload, "second_line_id");
      params.line_relations.push_back(relation);
    }
  }
  // Older saves predate the general constraints array — silently default
  // to none.
  if (payload.contains("constraints") && payload.at("constraints").is_array()) {
    for (const auto& constraint_payload : payload.at("constraints")) {
      polysmith::core::SketchConstraint constraint{};
      constraint.constraint_id = read_string(constraint_payload, "constraint_id");
      constraint.kind = read_string(constraint_payload, "kind");
      if (constraint_payload.contains("target_ids") &&
          constraint_payload.at("target_ids").is_array()) {
        for (const auto& tid : constraint_payload.at("target_ids")) {
          constraint.target_ids.push_back(tid.get<std::string>());
        }
      }
      if (constraint_payload.contains("value") &&
          constraint_payload.at("value").is_number()) {
        constraint.value = constraint_payload.at("value").get<double>();
      }
      if (constraint_payload.contains("driven") &&
          constraint_payload.at("driven").is_boolean()) {
        constraint.driven = constraint_payload.at("driven").get<bool>();
      }
      params.constraints.push_back(constraint);
    }
  }
  // Older saves predate midpoint anchors — silently default to none.
  if (payload.contains("midpoint_anchors") &&
      payload.at("midpoint_anchors").is_array()) {
    for (const auto& anchor_payload : payload.at("midpoint_anchors")) {
      polysmith::core::SketchMidpointAnchor anchor{};
      anchor.id = read_string(anchor_payload, "anchor_id");
      anchor.point_id = read_string(anchor_payload, "point_id");
      anchor.line_id = read_string(anchor_payload, "line_id");
      params.midpoint_anchors.push_back(anchor);
    }
  }
  // Older saves predate the Project tool's standalone projected
  // points — absence of these keys just means no projections exist.
  if (payload.contains("projected_points") &&
      payload.at("projected_points").is_array()) {
    for (const auto& point_payload : payload.at("projected_points")) {
      polysmith::core::SketchProjectedPoint projected{};
      projected.id = read_string(point_payload, "point_id");
      projected.source_id = read_string(point_payload, "source_id");
      projected.x = read_number(point_payload, "x");
      projected.y = read_number(point_payload, "y");
      params.projected_points.push_back(projected);
    }
  }
  if (payload.contains("projected_sources") &&
      payload.at("projected_sources").is_array()) {
    for (const auto& source_payload : payload.at("projected_sources")) {
      if (!source_payload.is_string()) {
        continue;
      }
      params.projected_sources.push_back(source_payload.get<std::string>());
    }
  }
  // Live-link projection records. Saves predating the live-link work
  // (or a save that came from a build with the old face-only Project
  // tool) won't include this key — those documents lose the live
  // link until the user re-projects, which gracefully degrades to
  // the previous "frozen at projection time" behaviour.
  if (payload.contains("projections") &&
      payload.at("projections").is_array()) {
    for (const auto& projection_payload : payload.at("projections")) {
      polysmith::core::SketchProjection projection{};
      projection.id = read_string(projection_payload, "projection_id");
      projection.source_id = read_string(projection_payload, "source_id");
      projection.source_kind = read_string(projection_payload, "source_kind");
      if (projection_payload.contains("generated_line_ids") &&
          projection_payload.at("generated_line_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_line_ids")) {
          if (id.is_string()) {
            projection.generated_line_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_circle_ids") &&
          projection_payload.at("generated_circle_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_circle_ids")) {
          if (id.is_string()) {
            projection.generated_circle_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_arc_ids") &&
          projection_payload.at("generated_arc_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_arc_ids")) {
          if (id.is_string()) {
            projection.generated_arc_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_point_id") &&
          projection_payload.at("generated_point_id").is_string()) {
        projection.generated_point_id =
            projection_payload.at("generated_point_id").get<std::string>();
      }
      if (projection_payload.contains("dependency_broken") &&
          projection_payload.at("dependency_broken").is_boolean()) {
        projection.dependency_broken =
            projection_payload.at("dependency_broken").get<bool>();
      }
      if (projection_payload.contains("dependency_warning") &&
          projection_payload.at("dependency_warning").is_string()) {
        projection.dependency_warning =
            projection_payload.at("dependency_warning").get<std::string>();
      }
      params.projections.push_back(std::move(projection));
    }
  }
  // Older saves predate point-line anchors — silently default to none.
  if (payload.contains("point_line_anchors") &&
      payload.at("point_line_anchors").is_array()) {
    for (const auto& anchor_payload : payload.at("point_line_anchors")) {
      polysmith::core::SketchPointLineAnchor anchor{};
      anchor.id = read_string(anchor_payload, "anchor_id");
      anchor.point_id = read_string(anchor_payload, "point_id");
      anchor.line_id = read_string(anchor_payload, "line_id");
      anchor.t = anchor_payload.contains("t") &&
                         anchor_payload.at("t").is_number()
                     ? anchor_payload.at("t").get<double>()
                     : 0.5;
      params.point_line_anchors.push_back(anchor);
    }
  }
  if (payload.contains("profiles") && payload.at("profiles").is_array()) {
    for (const auto& profile_payload : payload.at("profiles")) {
      polysmith::core::SketchProfileRegion profile{};
      profile.id = read_string(profile_payload, "profile_id");
      profile.kind = read_string(profile_payload, "kind");
      if (profile_payload.contains("point_ids") &&
          profile_payload.at("point_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("point_ids")) {
          profile.point_ids.push_back(id_value.get<std::string>());
        }
      }
      if (profile_payload.contains("line_ids") &&
          profile_payload.at("line_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("line_ids")) {
          profile.line_ids.push_back(id_value.get<std::string>());
        }
      }
      if (profile_payload.contains("points") &&
          profile_payload.at("points").is_array()) {
        for (const auto& pt_payload : profile_payload.at("points")) {
          profile.points.push_back(polysmith::core::SketchProfilePoint{
              .x = pt_payload.at("x").get<double>(),
              .y = pt_payload.at("y").get<double>(),
          });
        }
      }
      if (profile_payload.contains("inner_loops") &&
          profile_payload.at("inner_loops").is_array()) {
        for (const auto& loop_payload : profile_payload.at("inner_loops")) {
          std::vector<polysmith::core::SketchProfilePoint> loop;
          for (const auto& pt_payload : loop_payload) {
            loop.push_back(polysmith::core::SketchProfilePoint{
                .x = pt_payload.at("x").get<double>(),
                .y = pt_payload.at("y").get<double>(),
            });
          }
          profile.inner_loops.push_back(std::move(loop));
        }
      }
      profile.source_circle_id =
          read_optional_string(profile_payload, "source_circle_id");
      profile.center_x = read_number(profile_payload, "center_x");
      profile.center_y = read_number(profile_payload, "center_y");
      profile.radius = read_number(profile_payload, "radius");
      params.profiles.push_back(profile);
    }
  }
  // Pending mirror state. Older saves (and most live document
  // states outside an in-progress mirror) won't have this key —
  // that's fine, the field stays as `nullopt`.
  if (payload.contains("pending_mirror") &&
      payload.at("pending_mirror").is_object()) {
    polysmith::core::SketchFeatureParameters::PendingMirror pending{};
    const auto& pm = payload.at("pending_mirror");
    pending.axis_line_id = read_optional_string(pm, "axis_line_id");
    if (pm.contains("object_ids") && pm.at("object_ids").is_array()) {
      for (const auto& id_value : pm.at("object_ids")) {
        if (id_value.is_string()) {
          pending.object_ids.push_back(id_value.get<std::string>());
        }
      }
    }
    // Generated geometry is regenerated on every parameter
    // change, so it doesn't need to round-trip through saves —
    // but we still rebuild it from the payload for the live
    // viewport/document round-trip during a session.
    if (pm.contains("generated_lines") &&
        pm.at("generated_lines").is_array()) {
      for (const auto& line_payload : pm.at("generated_lines")) {
        polysmith::core::SketchLine line{};
        line.id = read_string(line_payload, "line_id");
        line.start_point_id = read_string(line_payload, "start_point_id");
        line.end_point_id = read_string(line_payload, "end_point_id");
        line.start_x = read_number(line_payload, "start_x");
        line.start_y = read_number(line_payload, "start_y");
        line.end_x = read_number(line_payload, "end_x");
        line.end_y = read_number(line_payload, "end_y");
        if (line_payload.contains("is_construction") &&
            line_payload.at("is_construction").is_boolean()) {
          line.is_construction = line_payload.at("is_construction").get<bool>();
        }
        pending.generated_lines.push_back(line);
      }
    }
    if (pm.contains("generated_circles") &&
        pm.at("generated_circles").is_array()) {
      for (const auto& circle_payload : pm.at("generated_circles")) {
        polysmith::core::SketchCircle circle{};
        circle.id = read_string(circle_payload, "circle_id");
        circle.center_x = read_number(circle_payload, "center_x");
        circle.center_y = read_number(circle_payload, "center_y");
        circle.radius = read_number(circle_payload, "radius");
        if (circle_payload.contains("is_construction") &&
            circle_payload.at("is_construction").is_boolean()) {
          circle.is_construction =
              circle_payload.at("is_construction").get<bool>();
        }
        pending.generated_circles.push_back(circle);
      }
    }
    params.pending_mirror = pending;
  }
  return params;
}

}  // namespace

polysmith::core::ExtrudeFeatureParameters
extrude_parameters_from_payload(const json& payload) {
  return parse_extrude_parameters_from_payload(payload);
}

json plane_frame_to_payload(const polysmith::core::PlaneFrame& frame) {
  return {
      {"origin",
       {
           {"x", frame.origin_x},
           {"y", frame.origin_y},
           {"z", frame.origin_z},
       }},
      {"x_axis",
       {
           {"x", frame.x_axis_x},
           {"y", frame.x_axis_y},
           {"z", frame.x_axis_z},
       }},
      {"y_axis",
       {
           {"x", frame.y_axis_x},
           {"y", frame.y_axis_y},
           {"z", frame.y_axis_z},
       }},
      {"normal",
       {
           {"x", frame.normal_x},
           {"y", frame.normal_y},
           {"z", frame.normal_z},
       }},
  };
}

json profile_points_to_payload(
    const std::vector<polysmith::core::SketchProfilePoint>& points) {
  json payload = json::array();
  for (const auto& point : points) {
    payload.push_back({{"x", point.x}, {"y", point.y}});
  }
  return payload;
}

// ── CAM Stock & Setup ───────────────────────────────────────────

json to_payload(const polysmith::core::CamStockDefinition& stock) {
  return {
      {"width", stock.width},
      {"height", stock.height},
      {"depth", stock.depth},
      {"offset_x", stock.offsetX},
      {"offset_y", stock.offsetY},
      {"offset_z", stock.offsetZ},
  };
}

json to_payload(const polysmith::core::CamSetup& setup) {
  return {
      {"setup_id", setup.id},
      {"stock", to_payload(setup.stock)},
      {"wcs_origin",
       {
           {"x", setup.wcsOriginX},
           {"y", setup.wcsOriginY},
           {"z", setup.wcsOriginZ},
       }},
      {"safety_plane_z", setup.safetyPlaneZ},
      {"axis_count", setup.axisCount},
      {"wcs_angle", setup.wcsAngle},
  };
}

polysmith::core::CamStockDefinition stock_from_payload(const json& payload) {
  polysmith::core::CamStockDefinition stock;
  if (payload.contains("width")) stock.width = payload["width"].get<double>();
  if (payload.contains("height")) stock.height = payload["height"].get<double>();
  if (payload.contains("depth")) stock.depth = payload["depth"].get<double>();
  if (payload.contains("offset_x")) stock.offsetX = payload["offset_x"].get<double>();
  if (payload.contains("offset_y")) stock.offsetY = payload["offset_y"].get<double>();
  if (payload.contains("offset_z")) stock.offsetZ = payload["offset_z"].get<double>();
  return stock;
}

json to_payload(const polysmith::core::CamToolDefinition& tool) {
  return {
      {"tool_id", tool.id},
      {"name", tool.name},
      {"tool_number", tool.toolNumber},
      {"diameter", tool.diameter},
      {"flute_length", tool.fluteLength},
      {"overall_length", tool.overallLength},
      {"corner_radius", tool.cornerRadius},
      {"spindle_speed", tool.spindleSpeed},
      {"feed_rate", tool.feedRate},
      {"stepover", tool.stepover},
      {"stepdown", tool.stepdown},
      {"type", static_cast<int>(tool.type)},
  };
}

polysmith::core::CamSetup setup_from_payload(const json& payload) {
  polysmith::core::CamSetup setup;
  if (payload.contains("setup_id")) setup.id = payload["setup_id"].get<std::string>();
  if (payload.contains("stock")) setup.stock = stock_from_payload(payload["stock"]);
  if (payload.contains("wcs_origin")) {
    const auto& wcs = payload["wcs_origin"];
    if (wcs.contains("x")) setup.wcsOriginX = wcs["x"].get<double>();
    if (wcs.contains("y")) setup.wcsOriginY = wcs["y"].get<double>();
    if (wcs.contains("z")) setup.wcsOriginZ = wcs["z"].get<double>();
  }
  if (payload.contains("safety_plane_z")) setup.safetyPlaneZ = payload["safety_plane_z"].get<double>();
  if (payload.contains("axis_count")) setup.axisCount = payload["axis_count"].get<int>();
  if (payload.contains("wcs_angle")) setup.wcsAngle = payload["wcs_angle"].get<double>();
  return setup;
}

polysmith::core::CamToolDefinition tool_from_payload(const json& payload) {
  polysmith::core::CamToolDefinition tool;
  if (payload.contains("tool_id")) tool.id = payload["tool_id"].get<std::string>();
  if (payload.contains("name")) tool.name = payload["name"].get<std::string>();
  if (payload.contains("tool_number")) tool.toolNumber = payload["tool_number"].get<int>();
  if (payload.contains("diameter")) tool.diameter = payload["diameter"].get<double>();
  if (payload.contains("flute_length")) tool.fluteLength = payload["flute_length"].get<double>();
  if (payload.contains("overall_length")) tool.overallLength = payload["overall_length"].get<double>();
  if (payload.contains("corner_radius")) tool.cornerRadius = payload["corner_radius"].get<double>();
  if (payload.contains("spindle_speed")) tool.spindleSpeed = payload["spindle_speed"].get<double>();
  if (payload.contains("feed_rate")) tool.feedRate = payload["feed_rate"].get<double>();
  if (payload.contains("stepover")) tool.stepover = payload["stepover"].get<double>();
  if (payload.contains("stepdown")) tool.stepdown = payload["stepdown"].get<double>();
  if (payload.contains("type")) {
    tool.type = static_cast<polysmith::core::CamToolDefinition::Type>(payload["type"].get<int>());
  }
  return tool;
}

json to_payload(const polysmith::core::FeatureEntry& feature) {
  return {
      {"feature_id", feature.id},
      {"kind", feature.kind},
      {"name", feature.name},
      {"status", feature.status},
      {"suppressed", feature.suppressed},
      // Surface broken-dependency state to the UI so the timeline can
      // render the warning. The frame on the feature itself stays at
      // its last-known value; this flag is the only signal that the
      // upstream face it referenced is gone.
      {"dependency_broken", feature.dependency_broken},
      {"dependency_warning", feature.dependency_warning},
      {"parameters_summary", feature.parameters_summary},
      {"box_parameters",
       feature.box_parameters.has_value()
           ? json{
                 {"width", feature.box_parameters->width},
                 {"height", feature.box_parameters->height},
                 {"depth", feature.box_parameters->depth},
             }
           : json(nullptr)},
      {"cylinder_parameters",
       feature.cylinder_parameters.has_value()
           ? json{
                 {"radius", feature.cylinder_parameters->radius},
                 {"height", feature.cylinder_parameters->height},
             }
           : json(nullptr)},
      {"extrude_parameters",
       feature.extrude_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.extrude_parameters->sketch_feature_id},
                 {"profile_id", feature.extrude_parameters->profile_id},
                 {"profile_ids",
                  [&feature]() {
                    json ids = json::array();
                    for (const auto& id : feature.extrude_parameters->profile_ids) {
                      ids.push_back(id);
                    }
                    return ids;
                  }()},
                 {"open_entity_ids",
                  [&feature]() {
                    json ids = json::array();
                    for (const auto& id :
                         feature.extrude_parameters->open_entity_ids) {
                      ids.push_back(id);
                    }
                    return ids;
                  }()},
                 {"plane_id", feature.extrude_parameters->plane_id},
                 {"plane_frame",
                  feature.extrude_parameters->plane_frame.has_value()
                      ? json{
                            {"origin",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->origin_x},
                                 {"y", feature.extrude_parameters->plane_frame->origin_y},
                                 {"z", feature.extrude_parameters->plane_frame->origin_z},
                             }},
                            {"x_axis",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->x_axis_x},
                                 {"y", feature.extrude_parameters->plane_frame->x_axis_y},
                                 {"z", feature.extrude_parameters->plane_frame->x_axis_z},
                             }},
                            {"y_axis",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->y_axis_x},
                                 {"y", feature.extrude_parameters->plane_frame->y_axis_y},
                                 {"z", feature.extrude_parameters->plane_frame->y_axis_z},
                             }},
                            {"normal",
                             {
                                 {"x", feature.extrude_parameters->plane_frame->normal_x},
                                 {"y", feature.extrude_parameters->plane_frame->normal_y},
                                 {"z", feature.extrude_parameters->plane_frame->normal_z},
                             }},
                        }
                      : json(nullptr)},
                 {"profile_kind", feature.extrude_parameters->profile_kind},
                 {"start_x", feature.extrude_parameters->start_x},
                 {"start_y", feature.extrude_parameters->start_y},
                 {"width", feature.extrude_parameters->width},
                 {"height", feature.extrude_parameters->height},
                 {"radius", feature.extrude_parameters->radius},
                 {"profile_points",
                  [&feature]() {
                    json points = json::array();
                    for (const auto& point :
                         feature.extrude_parameters->profile_points) {
                      points.push_back({
                          {"x", point.x},
                          {"y", point.y},
                      });
                    }
                    return points;
                  }()},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.extrude_parameters->inner_loops) {
                      json loop_payload = json::array();
                      for (const auto& point : loop) {
                        loop_payload.push_back({{"x", point.x}, {"y", point.y}});
                      }
                      loops.push_back(loop_payload);
                    }
                    return loops;
                  }()},
                 {"additional_profile_points",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile :
                         feature.extrude_parameters->additional_profile_points) {
                      json points = json::array();
                      for (const auto& point : profile) {
                        points.push_back({{"x", point.x}, {"y", point.y}});
                      }
                      profiles.push_back(points);
                    }
                    return profiles;
                  }()},
                 {"additional_inner_loops",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile_loops :
                         feature.extrude_parameters->additional_inner_loops) {
                      json loops = json::array();
                      for (const auto& loop : profile_loops) {
                        json loop_payload = json::array();
                        for (const auto& point : loop) {
                          loop_payload.push_back({{"x", point.x}, {"y", point.y}});
                        }
                        loops.push_back(loop_payload);
                      }
                      profiles.push_back(loops);
                    }
                    return profiles;
                 }()},
                 {"depth", feature.extrude_parameters->depth},
                 {"extent_mode", feature.extrude_parameters->extent_mode},
                 {"side1",
                  extrude_side_to_payload(feature.extrude_parameters->side1)},
                 {"side2",
                  feature.extrude_parameters->side2.has_value()
                      ? extrude_side_to_payload(
                            feature.extrude_parameters->side2.value())
                      : json(nullptr)},
                 {"thin",
                  {
                      {"enabled", feature.extrude_parameters->thin.enabled},
                      {"thickness", feature.extrude_parameters->thin.thickness},
                      {"placement", feature.extrude_parameters->thin.placement},
                  }},
                 {"mode", feature.extrude_parameters->mode},
                 {"operation", feature.extrude_parameters->operation},
                 {"intersect_result",
                  feature.extrude_parameters->intersect_result},
                 {"target_body_id",
                  feature.extrude_parameters->target_body_id.has_value()
                      ? json(feature.extrude_parameters->target_body_id.value())
                      : json(nullptr)},
             }
           : json(nullptr)},
      {"loft_parameters",
       feature.loft_parameters.has_value()
           ? json{
                 {"ruled", feature.loft_parameters->ruled},
                 {"sections",
                  [&feature]() {
                    json sections = json::array();
                    for (const auto& section :
                         feature.loft_parameters->sections) {
                      sections.push_back({
                          {"sketch_feature_id", section.sketch_feature_id},
                          {"profile_id", section.profile_id},
                          {"plane_id", section.plane_id},
                          {"plane_frame",
                           section.plane_frame.has_value()
                               ? plane_frame_to_payload(section.plane_frame.value())
                               : json(nullptr)},
                          {"profile_points",
                           profile_points_to_payload(section.profile_points)},
                      });
                    }
                    return sections;
                  }()},
             }
           : json(nullptr)},
      {"revolve_parameters",
       feature.revolve_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.revolve_parameters->sketch_feature_id},
                 {"profile_id", feature.revolve_parameters->profile_id},
                 {"plane_id", feature.revolve_parameters->plane_id},
                 {"plane_frame",
                  feature.revolve_parameters->plane_frame.has_value()
                      ? plane_frame_to_payload(
                            feature.revolve_parameters->plane_frame.value())
                      : json(nullptr)},
                 {"profile_kind", feature.revolve_parameters->profile_kind},
                 {"profile_points",
                  profile_points_to_payload(
                      feature.revolve_parameters->profile_points)},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.revolve_parameters->inner_loops) {
                      loops.push_back(profile_points_to_payload(loop));
                    }
                    return loops;
                  }()},
                 {"axis_sketch_feature_id",
                  feature.revolve_parameters->axis_sketch_feature_id},
                 {"axis_entity_id",
                  feature.revolve_parameters->axis_entity_id},
                 {"axis_start_x", feature.revolve_parameters->axis_start_x},
                 {"axis_start_y", feature.revolve_parameters->axis_start_y},
                 {"axis_start_z", feature.revolve_parameters->axis_start_z},
                 {"axis_end_x", feature.revolve_parameters->axis_end_x},
                 {"axis_end_y", feature.revolve_parameters->axis_end_y},
                 {"axis_end_z", feature.revolve_parameters->axis_end_z},
                 {"angle_degrees",
                  feature.revolve_parameters->angle_degrees},
             }
           : json(nullptr)},
      {"sweep_parameters",
       feature.sweep_parameters.has_value()
           ? json{
                 {"sketch_feature_id",
                  feature.sweep_parameters->sketch_feature_id},
                 {"profile_id", feature.sweep_parameters->profile_id},
                 {"plane_id", feature.sweep_parameters->plane_id},
                 {"plane_frame",
                  feature.sweep_parameters->plane_frame.has_value()
                      ? plane_frame_to_payload(
                            feature.sweep_parameters->plane_frame.value())
                      : json(nullptr)},
                 {"profile_kind", feature.sweep_parameters->profile_kind},
                 {"profile_points",
                  profile_points_to_payload(
                      feature.sweep_parameters->profile_points)},
                 {"inner_loops",
                  [&feature]() {
                    json loops = json::array();
                    for (const auto& loop :
                         feature.sweep_parameters->inner_loops) {
                      loops.push_back(profile_points_to_payload(loop));
                    }
                    return loops;
                  }()},
                 {"path_sketch_feature_id",
                  feature.sweep_parameters->path_sketch_feature_id},
                 {"path_entity_id",
                  feature.sweep_parameters->path_entity_id},
                 {"path_start_x", feature.sweep_parameters->path_start_x},
                 {"path_start_y", feature.sweep_parameters->path_start_y},
                 {"path_start_z", feature.sweep_parameters->path_start_z},
                 {"path_end_x", feature.sweep_parameters->path_end_x},
                 {"path_end_y", feature.sweep_parameters->path_end_y},
                 {"path_end_z", feature.sweep_parameters->path_end_z},
                 {"path_segments",
                  [&feature]() {
                    json segments = json::array();
                    for (const auto& segment :
                         feature.sweep_parameters->path_segments) {
                      segments.push_back(
                          json{{"entity_id", segment.entity_id},
                               {"kind", segment.kind},
                               {"start_x", segment.start_x},
                               {"start_y", segment.start_y},
                               {"start_z", segment.start_z},
                               {"end_x", segment.end_x},
                               {"end_y", segment.end_y},
                               {"end_z", segment.end_z},
                               {"center_x", segment.center_x},
                               {"center_y", segment.center_y},
                               {"center_z", segment.center_z},
                               {"mid_x", segment.mid_x},
                               {"mid_y", segment.mid_y},
                               {"mid_z", segment.mid_z},
                               {"radius", segment.radius},
                               {"ccw", segment.ccw}});
                    }
                    return segments;
                  }()},
             }
           : json(nullptr)},
      {"sketch_parameters",
       feature.sketch_parameters.has_value()
           ? json{
                 {"plane_id", feature.sketch_parameters->plane_id},
                 {"plane_frame",
                  feature.sketch_parameters->plane_frame.has_value()
                      ? json{
                            {"origin",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->origin_x},
                                 {"y", feature.sketch_parameters->plane_frame->origin_y},
                                 {"z", feature.sketch_parameters->plane_frame->origin_z},
                             }},
                            {"x_axis",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->x_axis_x},
                                 {"y", feature.sketch_parameters->plane_frame->x_axis_y},
                                 {"z", feature.sketch_parameters->plane_frame->x_axis_z},
                             }},
                            {"y_axis",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->y_axis_x},
                                 {"y", feature.sketch_parameters->plane_frame->y_axis_y},
                                 {"z", feature.sketch_parameters->plane_frame->y_axis_z},
                             }},
                            {"normal",
                             {
                                 {"x", feature.sketch_parameters->plane_frame->normal_x},
                                 {"y", feature.sketch_parameters->plane_frame->normal_y},
                                 {"z", feature.sketch_parameters->plane_frame->normal_z},
                             }},
                        }
                      : json(nullptr)},
                 {"lines",
                  [&feature]() {
                    json lines = json::array();
                    for (const auto& line : feature.sketch_parameters->lines) {
                      lines.push_back({
                          {"line_id", line.id},
                          {"start_point_id", line.start_point_id},
                          {"end_point_id", line.end_point_id},
                          {"start_x", line.start_x},
                          {"start_y", line.start_y},
                          {"end_x", line.end_x},
                          {"end_y", line.end_y},
                          {"constraint",
                           line.constraint.has_value()
                               ? json(line.constraint.value())
                               : json(nullptr)},
                          {"is_construction", line.is_construction},
                      });
                    }
                    return lines;
                  }()},
                 {"circles",
                 [&feature]() {
                    json circles = json::array();
                    for (const auto& circle : feature.sketch_parameters->circles) {
                      circles.push_back({
                          {"circle_id", circle.id},
                          {"center_x", circle.center_x},
                          {"center_y", circle.center_y},
                          {"radius", circle.radius},
                          {"is_construction", circle.is_construction},
                      });
                    }
                    return circles;
                  }()},
                 {"arcs",
                 [&feature]() {
                    json arcs = json::array();
                    for (const auto& arc : feature.sketch_parameters->arcs) {
                      arcs.push_back({
                          {"arc_id", arc.id},
                          {"start_point_id", arc.start_point_id},
                          {"end_point_id", arc.end_point_id},
                          {"center_x", arc.center_x},
                          {"center_y", arc.center_y},
                          {"radius", arc.radius},
                          {"start_x", arc.start_x},
                          {"start_y", arc.start_y},
                          {"end_x", arc.end_x},
                          {"end_y", arc.end_y},
                          {"ccw", arc.ccw},
                          {"is_construction", arc.is_construction},
                      });
                    }
                    return arcs;
                  }()},
                 {"fillets",
                  [&feature]() {
                    json fillets = json::array();
                    for (const auto& fillet : feature.sketch_parameters->fillets) {
                      fillets.push_back({
                          {"fillet_id", fillet.id},
                          {"corner_point_id", fillet.corner_point_id},
                          {"corner_x", fillet.corner_x},
                          {"corner_y", fillet.corner_y},
                          {"line_a_id", fillet.line_a_id},
                          {"line_b_id", fillet.line_b_id},
                          {"trim_a_point_id", fillet.trim_a_point_id},
                          {"trim_b_point_id", fillet.trim_b_point_id},
                          {"arc_id", fillet.arc_id},
                          {"radius", fillet.radius},
                      });
                    }
                    return fillets;
                  }()},
                  {"polygons",
                   [&feature]() {
                     json polygons = json::array();
                     for (const auto& polygon : feature.sketch_parameters->polygons) {
                       polygons.push_back({
                           {"polygon_id", polygon.id},
                           {"center_x", polygon.center_x},
                           {"center_y", polygon.center_y},
                           {"radius", polygon.radius},
                           {"sides", polygon.sides},
                           {"mode", polygon.mode},
                           {"start_x", polygon.start_x},
                           {"start_y", polygon.start_y},
                           {"end_x", polygon.end_x},
                           {"end_y", polygon.end_y},
                           {"is_construction", polygon.is_construction},
                       });
                     }
                     return polygons;
                   }()},
                  {"points",
                  [&feature]() {
                    json points = json::array();
                    for (const auto& point : feature.sketch_parameters->points) {
                      points.push_back({
                          {"point_id", point.id},
                          {"kind", point.kind},
                          {"x", point.x},
                          {"y", point.y},
                          {"is_fixed", point.is_fixed},
                      });
                    }
                    return points;
                  }()},
                 {"dimensions",
                  [&feature]() {
                    json dimensions = json::array();
                    for (const auto& dimension :
                         feature.sketch_parameters->dimensions) {
                      dimensions.push_back({
                          {"dimension_id", dimension.id},
                          {"kind", dimension.kind},
                          {"entity_id", dimension.entity_id},
                          {"secondary_entity_id",
                           dimension.secondary_entity_id},
                          {"value", dimension.value},
                          {"expression", dimension.expression},
                          {"driven", dimension.driven},
                          {"display_as", dimension.display_as},
                          {"label_x",
                           dimension.label_x.has_value()
                               ? json(*dimension.label_x)
                               : json(nullptr)},
                          {"label_y",
                           dimension.label_y.has_value()
                               ? json(*dimension.label_y)
                               : json(nullptr)},
                      });
                    }
                    return dimensions;
                  }()},
                 {"line_relations",
                  [&feature]() {
                    json relations = json::array();
                    for (const auto& relation :
                         feature.sketch_parameters->line_relations) {
                      relations.push_back({
                          {"relation_id", relation.id},
                          {"kind", relation.kind},
                          {"first_line_id", relation.first_line_id},
                          {"second_line_id", relation.second_line_id},
                        });
                    }
                    return relations;
                   }()},
                  {"constraints",
                   [&feature]() {
                     json constraint_list = json::array();
                     for (const auto& constraint :
                          feature.sketch_parameters->constraints) {
                       json target_ids = json::array();
                       for (const auto& tid : constraint.target_ids) {
                         target_ids.push_back(tid);
                       }
                       constraint_list.push_back({
                           {"constraint_id", constraint.constraint_id},
                           {"kind", constraint.kind},
                           {"target_ids", target_ids},
                           {"value", constraint.value},
                           {"driven", constraint.driven},
                         });
                     }
                     return constraint_list;
                   }()},
                  {"midpoint_anchors",
                  [&feature]() {
                    json anchors = json::array();
                    for (const auto& anchor :
                         feature.sketch_parameters->midpoint_anchors) {
                      anchors.push_back({
                          {"anchor_id", anchor.id},
                          {"point_id", anchor.point_id},
                          {"line_id", anchor.line_id},
                      });
                    }
                    return anchors;
                  }()},
                 {"point_line_anchors",
                  [&feature]() {
                    json anchors = json::array();
                    for (const auto& anchor :
                         feature.sketch_parameters->point_line_anchors) {
                      anchors.push_back({
                          {"anchor_id", anchor.id},
                          {"point_id", anchor.point_id},
                          {"line_id", anchor.line_id},
                          {"t", anchor.t},
                      });
                    }
                    return anchors;
                  }()},
                 {"projected_points",
                  [&feature]() {
                    json projected = json::array();
                    for (const auto& point :
                         feature.sketch_parameters->projected_points) {
                      projected.push_back({
                          {"point_id", point.id},
                          {"source_id", point.source_id},
                          {"x", point.x},
                          {"y", point.y},
                      });
                    }
                    return projected;
                  }()},
                 {"projected_sources",
                  [&feature]() {
                    json sources = json::array();
                    for (const auto& source :
                         feature.sketch_parameters->projected_sources) {
                      sources.push_back(source);
                    }
                    return sources;
                  }()},
                 {"projections",
                  [&feature]() {
                    json projections_payload = json::array();
                    for (const auto& projection :
                         feature.sketch_parameters->projections) {
                      json line_ids = json::array();
                      for (const auto& id : projection.generated_line_ids) {
                        line_ids.push_back(id);
                      }
                      json circle_ids = json::array();
                      for (const auto& id : projection.generated_circle_ids) {
                        circle_ids.push_back(id);
                      }
                      json arc_ids = json::array();
                      for (const auto& id : projection.generated_arc_ids) {
                        arc_ids.push_back(id);
                      }
                      projections_payload.push_back({
                          {"projection_id", projection.id},
                          {"source_id", projection.source_id},
                          {"source_kind", projection.source_kind},
                          {"generated_line_ids", line_ids},
                          {"generated_circle_ids", circle_ids},
                          {"generated_arc_ids", arc_ids},
                          {"generated_point_id",
                           projection.generated_point_id},
                          {"dependency_broken", projection.dependency_broken},
                          {"dependency_warning",
                           projection.dependency_warning},
                      });
                    }
                    return projections_payload;
                  }()},
                 {"profiles",
                  [&feature]() {
                    json profiles = json::array();
                    for (const auto& profile : feature.sketch_parameters->profiles) {
                      json point_ids = json::array();
                      for (const auto& point_id : profile.point_ids) {
                        point_ids.push_back(point_id);
                      }

                      json line_ids = json::array();
                      for (const auto& line_id : profile.line_ids) {
                        line_ids.push_back(line_id);
                      }

                      json points = json::array();
                      for (const auto& point : profile.points) {
                        points.push_back({
                            {"x", point.x},
                            {"y", point.y},
                        });
                      }
                      json inner_loops = json::array();
                      for (const auto& loop : profile.inner_loops) {
                        json loop_payload = json::array();
                        for (const auto& point : loop) {
                          loop_payload.push_back({
                              {"x", point.x},
                              {"y", point.y},
                          });
                        }
                        inner_loops.push_back(loop_payload);
                      }

                      profiles.push_back({
                          {"profile_id", profile.id},
                          {"kind", profile.kind},
                          {"point_ids", point_ids},
                          {"line_ids", line_ids},
                          {"points", points},
                          {"inner_loops", inner_loops},
                          {"source_circle_id",
                           profile.source_circle_id.has_value()
                               ? json(profile.source_circle_id.value())
                               : json(nullptr)},
                          {"center_x", profile.center_x},
                          {"center_y", profile.center_y},
                          {"radius", profile.radius},
                      });
                    }
                    return profiles;
                  }()},
                 {"active_tool", feature.sketch_parameters->active_tool},
                 // Pending mirror tool state. Emits `null` when no
                 // mirror is in progress; the UI uses presence of
                 // this object as the signal to show the floating
                 // panel.
                 {"pending_mirror",
                  feature.sketch_parameters->pending_mirror.has_value()
                      ? [&feature]() {
                          const auto& pm =
                              *feature.sketch_parameters->pending_mirror;
                          json object_ids = json::array();
                          for (const auto& id : pm.object_ids) {
                            object_ids.push_back(id);
                          }
                          json generated_lines = json::array();
                          for (const auto& line : pm.generated_lines) {
                            generated_lines.push_back({
                                {"line_id", line.id},
                                {"start_point_id", line.start_point_id},
                                {"end_point_id", line.end_point_id},
                                {"start_x", line.start_x},
                                {"start_y", line.start_y},
                                {"end_x", line.end_x},
                                {"end_y", line.end_y},
                                {"is_construction", line.is_construction},
                            });
                          }
                          json generated_circles = json::array();
                          for (const auto& circle : pm.generated_circles) {
                            generated_circles.push_back({
                                {"circle_id", circle.id},
                                {"center_x", circle.center_x},
                                {"center_y", circle.center_y},
                                {"radius", circle.radius},
                                {"is_construction", circle.is_construction},
                            });
                          }
                          return json{
                              {"axis_line_id",
                               pm.axis_line_id.has_value()
                                   ? json(*pm.axis_line_id)
                                   : json(nullptr)},
                              {"object_ids", object_ids},
                              {"generated_lines", generated_lines},
                              {"generated_circles", generated_circles},
                          };
                        }()
                      : json(nullptr)},
             }
           : json(nullptr)},
      {"fillet_parameters",
       feature.fillet_parameters.has_value()
           ? json{
                 {"target_body_id",
                  feature.fillet_parameters->target_body_id},
                 {"edge_ids", feature.fillet_parameters->edge_ids},
                 {"radius", feature.fillet_parameters->radius},
                 {"is_pending", feature.fillet_parameters->is_pending},
             }
           : json(nullptr)},
      {"chamfer_parameters",
       feature.chamfer_parameters.has_value()
           ? json{
                 {"target_body_id",
                  feature.chamfer_parameters->target_body_id},
                 {"edge_ids", feature.chamfer_parameters->edge_ids},
                 {"distance", feature.chamfer_parameters->distance},
                 {"is_pending", feature.chamfer_parameters->is_pending},
             }
           : json(nullptr)},
      {"shell_parameters",
       feature.shell_parameters.has_value()
           ? json{
                 {"target_body_id", feature.shell_parameters->target_body_id},
                 {"removed_face_ids",
                  feature.shell_parameters->removed_face_ids},
                 {"thickness", feature.shell_parameters->thickness},
                 {"is_pending", feature.shell_parameters->is_pending},
             }
           : json(nullptr)},
      {"construction_plane_parameters",
       feature.construction_plane_parameters.has_value()
           ? json{
                 {"plane_type",
                  feature.construction_plane_parameters->plane_type},
                 {"source_plane_id",
                 feature.construction_plane_parameters->source_plane_id},
                 {"source_plane_ids",
                  feature.construction_plane_parameters->source_plane_ids},
                 {"source_axis_id",
                  feature.construction_plane_parameters->source_axis_id},
                 {"offset", feature.construction_plane_parameters->offset},
                 {"angle_degrees",
                  feature.construction_plane_parameters->angle_degrees},
                 {"plane_frame",
                  {
                      {"origin",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.origin_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.origin_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.origin_z},
                       }},
                      {"x_axis",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.x_axis_z},
                       }},
                      {"y_axis",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.y_axis_z},
                       }},
                      {"normal",
                       {
                           {"x", feature.construction_plane_parameters
                                     ->plane_frame.normal_x},
                           {"y", feature.construction_plane_parameters
                                     ->plane_frame.normal_y},
                           {"z", feature.construction_plane_parameters
                                     ->plane_frame.normal_z},
                       }},
                  }},
             }
           : json(nullptr)},
      {"construction_axis_parameters",
       feature.construction_axis_parameters.has_value()
           ? json{
                 {"source_id", feature.construction_axis_parameters->source_id},
                 {"source_kind",
                  feature.construction_axis_parameters->source_kind},
                 {"start",
                  {{"x", feature.construction_axis_parameters->start_x},
                   {"y", feature.construction_axis_parameters->start_y},
                   {"z", feature.construction_axis_parameters->start_z}}},
                 {"end",
                  {{"x", feature.construction_axis_parameters->end_x},
                   {"y", feature.construction_axis_parameters->end_y},
                   {"z", feature.construction_axis_parameters->end_z}}},
             }
           : json(nullptr)},
      {"construction_point_parameters",
       feature.construction_point_parameters.has_value()
           ? json{
                 {"source_id", feature.construction_point_parameters->source_id},
                 {"source_kind",
                  feature.construction_point_parameters->source_kind},
                 {"position",
                  {{"x", feature.construction_point_parameters->x},
                   {"y", feature.construction_point_parameters->y},
                   {"z", feature.construction_point_parameters->z}}},
             }
           : json(nullptr)},
      {"hole_parameters",
       feature.hole_parameters.has_value()
           ? json{
                 {"target_body_id", feature.hole_parameters->target_body_id},
                 {"source_face_id", feature.hole_parameters->source_face_id},
                 {"plane_frame",
                  plane_frame_to_payload(feature.hole_parameters->plane_frame)},
                 {"center_x", feature.hole_parameters->center_x},
                 {"center_y", feature.hole_parameters->center_y},
                 {"hole_type", feature.hole_parameters->hole_type},
                 {"extent_type", feature.hole_parameters->extent_type},
                 {"diameter", feature.hole_parameters->diameter},
                 {"depth", feature.hole_parameters->depth},
                 {"counterbore_diameter",
                  feature.hole_parameters->counterbore_diameter},
                 {"counterbore_depth",
                  feature.hole_parameters->counterbore_depth},
                 {"countersink_diameter",
                  feature.hole_parameters->countersink_diameter},
                 {"countersink_angle_degrees",
                  feature.hole_parameters->countersink_angle_degrees},
                 {"standard", feature.hole_parameters->standard},
                 {"standard_size", feature.hole_parameters->standard_size},
                 {"hole_fit", feature.hole_parameters->hole_fit},
                 {"thread_enabled", feature.hole_parameters->thread_enabled},
                 {"thread_spec", feature.hole_parameters->thread_spec},
                 {"thread_pitch", feature.hole_parameters->thread_pitch},
                 {"major_diameter", feature.hole_parameters->major_diameter},
                 {"minor_diameter", feature.hole_parameters->minor_diameter},
                 {"thread_depth", feature.hole_parameters->thread_depth},
                 {"thread_representation",
                  feature.hole_parameters->thread_representation},
                 {"is_pending", feature.hole_parameters->is_pending},
             }
           : json(nullptr)},
      {"helix_parameters",
       feature.helix_parameters.has_value()
           ? json{
                 {"axis_source_id",
                  feature.helix_parameters->axis_source_id},
                 {"axis_start",
                  {{"x", feature.helix_parameters->axis_start_x},
                   {"y", feature.helix_parameters->axis_start_y},
                   {"z", feature.helix_parameters->axis_start_z}}},
                 {"axis_end",
                  {{"x", feature.helix_parameters->axis_end_x},
                   {"y", feature.helix_parameters->axis_end_y},
                   {"z", feature.helix_parameters->axis_end_z}}},
                 {"radius", feature.helix_parameters->radius},
                 {"pitch", feature.helix_parameters->pitch},
                 {"height", feature.helix_parameters->height},
                 {"turns", feature.helix_parameters->turns},
                 {"handedness", feature.helix_parameters->handedness},
                 {"start_angle_degrees",
                  feature.helix_parameters->start_angle_degrees},
                 {"points", feature.helix_parameters->points},
             }
           : json(nullptr)},
      {"thread_parameters",
       feature.thread_parameters.has_value()
           ? json{
                 {"target_body_id", feature.thread_parameters->target_body_id},
                 {"axis_source_id", feature.thread_parameters->axis_source_id},
                 {"mode", feature.thread_parameters->mode},
                 {"standard", feature.thread_parameters->standard},
                 {"size", feature.thread_parameters->size},
                 {"major_diameter",
                  feature.thread_parameters->major_diameter},
                 {"minor_diameter",
                  feature.thread_parameters->minor_diameter},
                 {"pitch", feature.thread_parameters->pitch},
                 {"length", feature.thread_parameters->length},
                 {"thread_angle_degrees",
                  feature.thread_parameters->thread_angle_degrees},
                 {"start_offset", feature.thread_parameters->start_offset},
                 {"handedness", feature.thread_parameters->handedness},
                 {"representation", feature.thread_parameters->representation},
                 {"is_pending", feature.thread_parameters->is_pending},
             }
           : json(nullptr)},
      {"fastener_parameters",
       feature.fastener_parameters.has_value()
           ? json{
                 {"standard", feature.fastener_parameters->standard},
                 {"size", feature.fastener_parameters->size},
                 {"diameter", feature.fastener_parameters->diameter},
                 {"minor_diameter",
                  feature.fastener_parameters->minor_diameter},
                 {"pitch", feature.fastener_parameters->pitch},
                 {"length", feature.fastener_parameters->length},
                 {"thread_length", feature.fastener_parameters->thread_length},
                 {"head_type", feature.fastener_parameters->head_type},
                 {"drive_type", feature.fastener_parameters->drive_type},
                 {"thread_representation",
                  feature.fastener_parameters->thread_representation},
             }
           : json(nullptr)},
      {"move_parameters",
       feature.move_parameters.has_value()
           ? json{
                 {"target_body_id", feature.move_parameters->target_body_id},
                 {"translation_x", feature.move_parameters->translation_x},
                 {"translation_y", feature.move_parameters->translation_y},
                 {"translation_z", feature.move_parameters->translation_z},
                 {"rotation_x_degrees",
                  feature.move_parameters->rotation_x_degrees},
                 {"rotation_y_degrees",
                  feature.move_parameters->rotation_y_degrees},
                 {"rotation_z_degrees",
                  feature.move_parameters->rotation_z_degrees},
                 {"is_pending", feature.move_parameters->is_pending},
             }
           : json(nullptr)},
      {"body_copy_parameters",
       feature.body_copy_parameters.has_value()
           ? json{
                 {"source_body_id",
                  feature.body_copy_parameters->source_body_id},
                 {"copy_mode", feature.body_copy_parameters->copy_mode},
                 {"source_body_name",
                  feature.body_copy_parameters->source_body_name},
                 {"serialized_shape",
                  feature.body_copy_parameters->serialized_shape},
                 {"local_x_axis_x",
                  feature.body_copy_parameters->local_x_axis_x},
                 {"local_x_axis_y",
                  feature.body_copy_parameters->local_x_axis_y},
                 {"local_x_axis_z",
                  feature.body_copy_parameters->local_x_axis_z},
                 {"local_y_axis_x",
                  feature.body_copy_parameters->local_y_axis_x},
                 {"local_y_axis_y",
                  feature.body_copy_parameters->local_y_axis_y},
                 {"local_y_axis_z",
                  feature.body_copy_parameters->local_y_axis_z},
                 {"local_z_axis_x",
                  feature.body_copy_parameters->local_z_axis_x},
                 {"local_z_axis_y",
                  feature.body_copy_parameters->local_z_axis_y},
                 {"local_z_axis_z",
                  feature.body_copy_parameters->local_z_axis_z},
             }
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::DocumentState& document) {
  json feature_history = json::array();
  for (const auto& feature : document.feature_history) {
    feature_history.push_back(to_payload(feature));
  }

  return {
      {"document_id", document.id},
      {"name", document.name},
      {"units", document.units},
      {"revision", document.revision},
      {"selected_feature_id",
       document.selected_feature_id.has_value()
           ? json(document.selected_feature_id.value())
           : json(nullptr)},
      {"selected_reference_id",
       document.selected_reference_id.has_value()
           ? json(document.selected_reference_id.value())
           : json(nullptr)},
      {"selected_face_id",
       document.selected_face_id.has_value()
           ? json(document.selected_face_id.value())
           : json(nullptr)},
      {"selected_edge_ids", document.selected_edge_ids},
      {"selected_vertex_ids", document.selected_vertex_ids},
      {"active_sketch_plane_id",
       document.active_sketch_plane_id.has_value()
           ? json(document.active_sketch_plane_id.value())
           : json(nullptr)},
      {"active_sketch_face_id",
       document.active_sketch_face_id.has_value()
           ? json(document.active_sketch_face_id.value())
           : json(nullptr)},
      {"active_sketch_feature_id",
       document.active_sketch_feature_id.has_value()
           ? json(document.active_sketch_feature_id.value())
           : json(nullptr)},
      {"active_sketch_tool",
       document.active_sketch_tool.has_value()
           ? json(document.active_sketch_tool.value())
           : json(nullptr)},
      {"selected_sketch_point_id",
       document.selected_sketch_point_id.has_value()
           ? json(document.selected_sketch_point_id.value())
           : json(nullptr)},
      {"selected_sketch_entity_id",
       document.selected_sketch_entity_id.has_value()
           ? json(document.selected_sketch_entity_id.value())
           : json(nullptr)},
      {"selected_sketch_point_ids", document.selected_sketch_point_ids},
      {"selected_sketch_entity_ids", document.selected_sketch_entity_ids},
      {"selected_sketch_dimension_id",
       document.selected_sketch_dimension_id.has_value()
           ? json(document.selected_sketch_dimension_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_id",
       document.selected_sketch_profile_id.has_value()
           ? json(document.selected_sketch_profile_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_ids",
       [&document]() {
         json ids = json::array();
         for (const auto& id : document.selected_sketch_profile_ids) {
           ids.push_back(id);
         }
         return ids;
       }()},
      {"timeline_cursor",
       document.timeline_cursor.has_value()
           ? json(document.timeline_cursor.value())
           : json(nullptr)},
      {"feature_history", feature_history},
      {"parameters",
       [&document]() {
         json params = json::array();
         for (const auto& p : document.parameters) {
            params.push_back({
                {"name", p.name},
                {"expression", p.expression},
                {"resolved_value", p.resolved_value},
                {"kind", p.kind},
                {"has_error", p.has_error},
                {"error_message", p.error_message},
            });
         }
        return params;
        }()},
       {"appearance",
        [&document]() {
          json body_colors = json::array();
          for (const auto& entry : document.appearance.body_colors) {
            body_colors.push_back({
                {"body_id", entry.body_id},
                {"color", entry.color},
            });
          }
          json face_colors = json::array();
          for (const auto& entry : document.appearance.face_colors) {
            face_colors.push_back({
                {"face_id", entry.face_id},
                {"owner_body_id", entry.owner_body_id},
                {"signature", entry.signature},
                {"color", entry.color},
            });
          }
          return json{
              {"body_colors", body_colors},
              {"face_colors", face_colors},
          };
        }()},
       {"selection_filter",
        [&document]() {
          const auto& sf = document.selection_filter;
          return json{
              {"select_curves", sf.select_curves},
              {"select_points", sf.select_points},
              {"select_construction", sf.select_construction},
              {"select_constraints", sf.select_constraints},
              {"snap_endpoint", sf.snap_endpoint},
              {"snap_midpoint", sf.snap_midpoint},
              {"snap_center", sf.snap_center},
              {"snap_intersection", sf.snap_intersection},
              {"snap_nearest", sf.snap_nearest},
              {"snap_quadrant", sf.snap_quadrant},
              {"snap_perpendicular", sf.snap_perpendicular},
              {"snap_parallel", sf.snap_parallel},
              {"snap_tangent", sf.snap_tangent},
              {"snap_grid", sf.snap_grid},
              {"snap_grid_line", sf.snap_grid_line},
              {"snap_polar", sf.snap_polar},
              {"polar_angle_degrees", sf.polar_angle_degrees},
              {"magnetic_pull", sf.magnetic_pull},
              {"tolerance_px", sf.tolerance_px},
          };
        }()},
       {"cam_setup",
        document.cam_setup.has_value()
            ? to_payload(document.cam_setup.value())
            : json(nullptr)},
       {"tool_library", [&document]() {
          json tools = json::array();
          for (const auto& tool : document.tool_library) {
            tools.push_back(to_payload(tool));
          }
          return tools;
        }()},
       {"cam_operations", [&document]() {
          json ops = json::array();
          for (const auto& op : document.cam_operations) {
            json opJson = {
                {"id", op.id},
                {"name", op.name},
                {"type", static_cast<int>(op.type)},
                {"tool_id", op.toolId},
                {"body_id", op.bodyId},
                {"face_index", op.faceIndex},
            };
            if (op.faceMilling.has_value()) {
              opJson["face_milling"] = {
                  {"depth", op.faceMilling->depth},
                  {"stepover", op.faceMilling->stepover},
                  {"angle_deg", op.faceMilling->angleDeg},
              };
            }
            ops.push_back(opJson);
          }
          return ops;
        }()},
   };
}

json to_payload(const polysmith::core::SessionState& session) {
  json payload = {
      {"document_count", session.document_count},
      {"has_active_document", session.active_document_id.has_value()},
      {"can_undo", session.can_undo},
      {"can_redo", session.can_redo},
  };

  if (session.active_document_id.has_value()) {
    payload["active_document_id"] = session.active_document_id.value();
  } else {
    payload["active_document_id"] = nullptr;
  }

  return payload;
}

json to_payload(const polysmith::core::ViewportBoxPrimitive& primitive) {
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"width", primitive.width},
      {"height", primitive.height},
      {"depth", primitive.depth},
      {"x_offset", primitive.x_offset},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
      }},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportCylinderPrimitive& primitive) {
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"radius", primitive.radius},
      {"height", primitive.height},
      {"x_offset", primitive.x_offset},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
      }},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportPolygonExtrudePrimitive& primitive) {
  json profile_points = json::array();
  for (const auto& point : primitive.profile_points) {
    profile_points.push_back({
        {"x", point.x},
        {"y", point.y},
    });
  }
  json inner_loops = json::array();
  for (const auto& loop : primitive.inner_loops) {
    json loop_payload = json::array();
    for (const auto& point : loop) {
      loop_payload.push_back({
          {"x", point.x},
          {"y", point.y},
      });
    }
    inner_loops.push_back(loop_payload);
  }

  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? json{
                 {"origin",
                  {
                      {"x", primitive.plane_frame->origin_x},
                      {"y", primitive.plane_frame->origin_y},
                      {"z", primitive.plane_frame->origin_z},
                  }},
                 {"x_axis",
                  {
                      {"x", primitive.plane_frame->x_axis_x},
                      {"y", primitive.plane_frame->x_axis_y},
                      {"z", primitive.plane_frame->x_axis_z},
                  }},
                 {"y_axis",
                  {
                      {"x", primitive.plane_frame->y_axis_x},
                      {"y", primitive.plane_frame->y_axis_y},
                      {"z", primitive.plane_frame->y_axis_z},
                  }},
                 {"normal",
                  {
                      {"x", primitive.plane_frame->normal_x},
                      {"y", primitive.plane_frame->normal_y},
                      {"z", primitive.plane_frame->normal_z},
                  }},
             }
           : json(nullptr)},
      {"profile_points", profile_points},
      {"depth", primitive.depth},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportSolidFace& face) {
  return {
      {"face_id", face.face_id},
      {"owner_id", face.owner_id},
      {"owner_kind", face.owner_kind},
      {"label", face.label},
      {"sketchability", face.sketchability},
      {"center",
       {
           {"x", face.center_x},
           {"y", face.center_y},
           {"z", face.center_z},
       }},
      {"normal",
       {
           {"x", face.normal_x},
           {"y", face.normal_y},
           {"z", face.normal_z},
       }},
      {"plane_frame",
       {
           {"origin",
            {
                {"x", face.plane_frame.origin_x},
                {"y", face.plane_frame.origin_y},
                {"z", face.plane_frame.origin_z},
            }},
           {"x_axis",
            {
                {"x", face.plane_frame.x_axis_x},
                {"y", face.plane_frame.x_axis_y},
                {"z", face.plane_frame.x_axis_z},
            }},
           {"y_axis",
            {
                {"x", face.plane_frame.y_axis_x},
                {"y", face.plane_frame.y_axis_y},
                {"z", face.plane_frame.y_axis_z},
            }},
           {"normal",
            {
                {"x", face.plane_frame.normal_x},
                {"y", face.plane_frame.normal_y},
                {"z", face.plane_frame.normal_z},
            }},
       }},
      {"size",
       {
           {"width", face.width},
           {"height", face.height},
           {"radius", face.radius},
       }},
      {"triangle_positions", face.triangle_positions},
      {"triangle_indices", face.triangle_indices},
      {"is_selected", face.is_selected},
      {"appearance_color",
       face.appearance_color.has_value()
           ? json(face.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportReferencePlane& plane) {
  return {
      {"reference_id", plane.id},
      {"label", plane.label},
      {"orientation", plane.orientation},
      {"center",
       {
           {"x", plane.center_x},
           {"y", plane.center_y},
           {"z", plane.center_z},
       }},
      {"size",
       {
           {"width", plane.width},
           {"height", plane.height},
       }},
      {"is_selected", plane.is_selected},
      {"is_active_sketch_plane", plane.is_active_sketch_plane},
      // Construction planes ship a real frame; origin planes leave
      // it null and the renderer falls back to its hardcoded
      // orientation rotation.
      {"plane_frame",
       plane.plane_frame.has_value()
           ? json{
                 {"origin",
                  {
                      {"x", plane.plane_frame->origin_x},
                      {"y", plane.plane_frame->origin_y},
                      {"z", plane.plane_frame->origin_z},
                  }},
                 {"x_axis",
                  {
                      {"x", plane.plane_frame->x_axis_x},
                      {"y", plane.plane_frame->x_axis_y},
                      {"z", plane.plane_frame->x_axis_z},
                  }},
                 {"y_axis",
                  {
                      {"x", plane.plane_frame->y_axis_x},
                      {"y", plane.plane_frame->y_axis_y},
                      {"z", plane.plane_frame->y_axis_z},
                  }},
                 {"normal",
                  {
                      {"x", plane.plane_frame->normal_x},
                      {"y", plane.plane_frame->normal_y},
                      {"z", plane.plane_frame->normal_z},
                  }},
             }
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportReferenceAxis& axis) {
  return {
      {"reference_id", axis.id},
      {"label", axis.label},
      {"axis", axis.axis},
      {"start",
       {
           {"x", axis.start_x},
           {"y", axis.start_y},
           {"z", axis.start_z},
       }},
      {"end",
       {
           {"x", axis.end_x},
           {"y", axis.end_y},
           {"z", axis.end_z},
       }},
  };
}

json to_payload(const polysmith::core::ViewportReferencePoint& point) {
  return {
      {"reference_id", point.id},
      {"label", point.label},
      {"position", {{"x", point.x}, {"y", point.y}, {"z", point.z}}},
      {"is_selected", point.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportHelixPrimitive& helix) {
  return {
      {"helix_id", helix.id},
      {"label", helix.label},
      {"points", helix.points},
      {"is_selected", helix.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportSketchLinePrimitive& primitive) {
  return {
      {"line_id", primitive.line_id},
      {"start_point_id", primitive.start_point_id},
      {"end_point_id", primitive.end_point_id},
      {"plane_id", primitive.plane_id},
      {"start",
       {
           {"x", primitive.start_x},
           {"y", primitive.start_y},
           {"z", primitive.start_z},
       }},
      {"end",
       {
           {"x", primitive.end_x},
           {"y", primitive.end_y},
           {"z", primitive.end_z},
       }},
      {"is_selected", primitive.is_selected},
      {"constraint",
       primitive.constraint.has_value()
           ? json(primitive.constraint.value())
           : json(nullptr)},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchPolygonPrimitive& primitive) {
  return {
      {"polygon_id", primitive.polygon_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame", primitive.plane_frame.has_value() ? viewport_sketch_plane_frame_to_payload(primitive.plane_frame.value()) : json(nullptr)},
      {"corner_x", primitive.corner_x},
      {"corner_y", primitive.corner_y},
      {"corner_z", primitive.corner_z},
      {"sides", primitive.sides},
      {"mode", primitive.mode},
      {"center", {{"x", primitive.center_x}, {"y", primitive.center_y}, {"z", primitive.center_z}}},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchCirclePrimitive& primitive) {
  return {
      {"circle_id", primitive.circle_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? viewport_sketch_plane_frame_to_payload(primitive.plane_frame.value())
           : json(nullptr)},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
       }},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchArcPrimitive& primitive) {
  return {
      {"arc_id", primitive.arc_id},
      {"start_point_id", primitive.start_point_id},
      {"end_point_id", primitive.end_point_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? viewport_sketch_plane_frame_to_payload(primitive.plane_frame.value())
           : json(nullptr)},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
       }},
      {"radius", primitive.radius},
      {"start",
       {
           {"x", primitive.start_x},
           {"y", primitive.start_y},
           {"z", primitive.start_z},
       }},
      {"end",
       {
           {"x", primitive.end_x},
           {"y", primitive.end_y},
           {"z", primitive.end_z},
       }},
      {"ccw", primitive.ccw},
      {"is_selected", primitive.is_selected},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchPointPrimitive& primitive) {
  return {
      {"point_id", primitive.point_id},
      {"plane_id", primitive.plane_id},
      {"kind", primitive.kind},
      {"position",
       {
           {"x", primitive.position_x},
           {"y", primitive.position_y},
           {"z", primitive.position_z},
       }},
      {"is_fixed", primitive.is_fixed},
      {"is_selected", primitive.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportSketchDimensionPrimitive& primitive) {
  return {
      {"dimension_id", primitive.dimension_id},
      {"plane_id", primitive.plane_id},
      {"kind", primitive.kind},
      {"entity_id", primitive.entity_id},
      {"label", primitive.label},
      {"is_selected", primitive.is_selected},
      {"anchor_start",
       {
           {"x", primitive.anchor_start_x},
           {"y", primitive.anchor_start_y},
           {"z", primitive.anchor_start_z},
       }},
      {"anchor_end",
       {
           {"x", primitive.anchor_end_x},
           {"y", primitive.anchor_end_y},
           {"z", primitive.anchor_end_z},
       }},
      {"dimension_start",
       {
           {"x", primitive.dimension_start_x},
           {"y", primitive.dimension_start_y},
           {"z", primitive.dimension_start_z},
       }},
      {"dimension_end",
       {
           {"x", primitive.dimension_end_x},
           {"y", primitive.dimension_end_y},
           {"z", primitive.dimension_end_z},
       }},
      {"label_position",
       {
           {"x", primitive.label_x},
           {"y", primitive.label_y},
           {"z", primitive.label_z},
       }},
      {"arc_center",
       {
           {"x", primitive.arc_center_x},
           {"y", primitive.arc_center_y},
           {"z", primitive.arc_center_z},
       }},
      {"arc_radius", primitive.arc_radius},
      {"arc_start_angle", primitive.arc_start_angle},
      {"arc_end_angle", primitive.arc_end_angle},
      {"arc_ccw", primitive.arc_ccw},
      {"ref_line_start",
       {
           {"x", primitive.ref_line_start_x},
           {"y", primitive.ref_line_start_y},
           {"z", primitive.ref_line_start_z},
       }},
      {"ref_line_end",
       {
           {"x", primitive.ref_line_end_x},
           {"y", primitive.ref_line_end_y},
           {"z", primitive.ref_line_end_z},
       }},
  };
}

json to_payload(const polysmith::core::ViewportSketchConstraintPrimitive& primitive) {
  return {
      {"constraint_id", primitive.constraint_id},
      {"plane_id", primitive.plane_id},
      {"kind", primitive.kind},
      {"entity_id", primitive.entity_id},
      {"related_entity_id",
       primitive.related_entity_id.has_value()
           ? json(primitive.related_entity_id.value())
           : json(nullptr)},
      {"label", primitive.label},
      {"is_selected", primitive.is_selected},
      {"position",
       {
           {"x", primitive.position_x},
           {"y", primitive.position_y},
           {"z", primitive.position_z},
       }},
  };
}

json to_payload(const polysmith::core::ViewportSketchProfilePrimitive& primitive) {
  json profile_points = json::array();
  for (const auto& point : primitive.profile_points) {
    profile_points.push_back({
        {"x", point.x},
        {"y", point.y},
    });
  }
  json inner_loops = json::array();
  for (const auto& loop : primitive.inner_loops) {
    json loop_payload = json::array();
    for (const auto& point : loop) {
      loop_payload.push_back({
          {"x", point.x},
          {"y", point.y},
      });
    }
    inner_loops.push_back(loop_payload);
  }

  return {
      {"profile_id", primitive.profile_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? json{
                 {"origin",
                  {
                      {"x", primitive.plane_frame->origin_x},
                      {"y", primitive.plane_frame->origin_y},
                      {"z", primitive.plane_frame->origin_z},
                  }},
                 {"x_axis",
                  {
                      {"x", primitive.plane_frame->x_axis_x},
                      {"y", primitive.plane_frame->x_axis_y},
                      {"z", primitive.plane_frame->x_axis_z},
                  }},
                 {"y_axis",
                  {
                      {"x", primitive.plane_frame->y_axis_x},
                      {"y", primitive.plane_frame->y_axis_y},
                      {"z", primitive.plane_frame->y_axis_z},
                  }},
                 {"normal",
                  {
                      {"x", primitive.plane_frame->normal_x},
                      {"y", primitive.plane_frame->normal_y},
                      {"z", primitive.plane_frame->normal_z},
                  }},
             }
           : json(nullptr)},
      {"profile_kind", primitive.profile_kind},
      {"profile_points", profile_points},
      {"inner_loops", inner_loops},
      {"start_x", primitive.start_x},
      {"start_y", primitive.start_y},
      {"width", primitive.width},
      {"height", primitive.height},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportToolpathPrimitive& primitive) {
  json points = json::array();
  for (const auto& p : primitive.points) {
    points.push_back({
        {"x", p.x},
        {"y", p.y},
        {"z", p.z},
        {"is_rapid", p.is_rapid},
    });
  }
  return {
      {"id", primitive.id},
      {"label", primitive.label},
      {"points", points},
  };
}

json to_payload(const polysmith::core::ViewportState& viewport) {
  json boxes = json::array();
  for (const auto& box : viewport.boxes) {
    boxes.push_back(to_payload(box));
  }
  json solid_faces = json::array();
  for (const auto& face : viewport.solid_faces) {
    solid_faces.push_back(to_payload(face));
  }

  json cylinders = json::array();
  for (const auto& cylinder : viewport.cylinders) {
    cylinders.push_back(to_payload(cylinder));
  }

  json polygon_extrudes = json::array();
  for (const auto& polygon_extrude : viewport.polygon_extrudes) {
    polygon_extrudes.push_back(to_payload(polygon_extrude));
  }

  json reference_planes = json::array();
  for (const auto& plane : viewport.reference_planes) {
    reference_planes.push_back(to_payload(plane));
  }

  json reference_axes = json::array();
  for (const auto& axis : viewport.reference_axes) {
    reference_axes.push_back(to_payload(axis));
  }

  json reference_points = json::array();
  for (const auto& point : viewport.reference_points) {
    reference_points.push_back(to_payload(point));
  }

  json helices = json::array();
  for (const auto& helix : viewport.helices) {
    helices.push_back(to_payload(helix));
  }

  json sketch_lines = json::array();
  for (const auto& line : viewport.sketch_lines) {
    sketch_lines.push_back(to_payload(line));
  }

  json sketch_circles = json::array();
  for (const auto& circle : viewport.sketch_circles) {
    sketch_circles.push_back(to_payload(circle));
  }

  json sketch_polygons = json::array();
  for (const auto& polygon : viewport.sketch_polygons) {
    sketch_polygons.push_back(to_payload(polygon));
  }

  json sketch_arcs = json::array();
  for (const auto& arc : viewport.sketch_arcs) {
    sketch_arcs.push_back(to_payload(arc));
  }

  json sketch_points = json::array();
  for (const auto& point : viewport.sketch_points) {
    sketch_points.push_back(to_payload(point));
  }

  json sketch_dimensions = json::array();
  for (const auto& dimension : viewport.sketch_dimensions) {
    sketch_dimensions.push_back(to_payload(dimension));
  }

  json sketch_constraints = json::array();
  for (const auto& constraint : viewport.sketch_constraints) {
    sketch_constraints.push_back(to_payload(constraint));
  }

  json sketch_profiles = json::array();
  for (const auto& profile : viewport.sketch_profiles) {
    sketch_profiles.push_back(to_payload(profile));
  }

  json meshes = json::array();
  for (const auto& mesh : viewport.meshes) {
    meshes.push_back({
        {"primitive_id", mesh.id},
        {"positions", mesh.positions},
        {"normals", mesh.normals},
        {"indices", mesh.indices},
        {"is_selected", mesh.is_selected},
        {"appearance_color",
         mesh.appearance_color.has_value()
             ? json(mesh.appearance_color.value())
             : json(nullptr)},
    });
  }

  return {
      {"has_active_document", viewport.has_active_document},
      {"boxes", boxes},
      {"cylinders", cylinders},
      {"polygon_extrudes", polygon_extrudes},
      {"solid_faces", solid_faces},
      {"reference_planes", reference_planes},
      {"reference_axes", reference_axes},
      {"reference_points", reference_points},
      {"helices", helices},
      {"sketch_lines", sketch_lines},
      {"sketch_circles", sketch_circles},
      {"sketch_polygons", sketch_polygons},
      {"sketch_arcs", sketch_arcs},
      {"sketch_points", sketch_points},
      {"sketch_dimensions", sketch_dimensions},
      {"sketch_constraints", sketch_constraints},
      {"sketch_profiles", sketch_profiles},
      {"meshes", meshes},
      {"toolpaths", [&viewport]() {
         json toolpaths_json = json::array();
         for (const auto& tp : viewport.toolpaths) {
           toolpaths_json.push_back(to_payload(tp));
         }
         return toolpaths_json;
       }()},
      {"cut_previews", [&viewport]() {
         json previews = json::array();
         for (const auto& preview : viewport.cut_previews) {
           previews.push_back({
               {"id", preview.id},
               {"positions", preview.positions},
               {"normals", preview.normals},
               {"indices", preview.indices},
           });
         }
         return previews;
       }()},
      {"bodies", [&viewport]() {
         json bodies_json = json::array();
         for (const auto& body : viewport.bodies) {
           bodies_json.push_back({
               {"id", body.id},
               {"label", body.label},
               {"center",
                {{"x", body.center_x},
                 {"y", body.center_y},
                 {"z", body.center_z}}},
               {"size",
                {{"x", body.width},
                 {"y", body.height},
                 {"z", body.depth}}},
               {"local_frame",
                {
                    {"x_axis",
                     {{"x", body.local_frame.x_axis_x},
                      {"y", body.local_frame.x_axis_y},
                      {"z", body.local_frame.x_axis_z}}},
                    {"y_axis",
                     {{"x", body.local_frame.y_axis_x},
                      {"y", body.local_frame.y_axis_y},
                      {"z", body.local_frame.y_axis_z}}},
                    {"z_axis",
                     {{"x", body.local_frame.z_axis_x},
                      {"y", body.local_frame.z_axis_y},
                      {"z", body.local_frame.z_axis_z}}},
                }},
           });
         }
         return bodies_json;
       }()},
      {"edges", [&viewport]() {
         json edges_json = json::array();
         for (const auto& edge : viewport.edges) {
           edges_json.push_back({
               {"id", edge.id},
               {"owner_body_id", edge.owner_body_id},
               {"kind", edge.kind},
               {"points", edge.points},
               {"length", edge.length},
               {"is_selected", edge.is_selected},
           });
         }
         return edges_json;
       }()},
      {"vertices", [&viewport]() {
         json vertices_json = json::array();
         for (const auto& vertex : viewport.vertices) {
           vertices_json.push_back({
               {"id", vertex.id},
               {"owner_body_id", vertex.owner_body_id},
               {"position",
                {
                    {"x", vertex.x},
                    {"y", vertex.y},
                    {"z", vertex.z},
                }},
               {"is_selected", vertex.is_selected},
           });
         }
         return vertices_json;
       }()},
      {"scene_width", viewport.scene_width},
      {"scene_height", viewport.scene_height},
      {"scene_depth", viewport.scene_depth},
      {"scene_bounds",
       {
           {"center",
            {
                {"x", viewport.scene_bounds.center_x},
                {"y", viewport.scene_bounds.center_y},
                {"z", viewport.scene_bounds.center_z},
            }},
           {"size",
            {
                {"x", viewport.scene_bounds.width},
                {"y", viewport.scene_bounds.height},
                {"z", viewport.scene_bounds.depth},
            }},
            {"max_dimension", viewport.scene_bounds.max_dimension},
        }},
       {"selection_filter",
        [&viewport]() {
          const auto& sf = viewport.selection_filter;
          return json{
              {"select_curves", sf.select_curves},
              {"select_points", sf.select_points},
              {"select_construction", sf.select_construction},
              {"select_constraints", sf.select_constraints},
              {"snap_endpoint", sf.snap_endpoint},
              {"snap_midpoint", sf.snap_midpoint},
              {"snap_center", sf.snap_center},
              {"snap_intersection", sf.snap_intersection},
              {"snap_nearest", sf.snap_nearest},
              {"snap_quadrant", sf.snap_quadrant},
              {"snap_perpendicular", sf.snap_perpendicular},
              {"snap_parallel", sf.snap_parallel},
              {"snap_tangent", sf.snap_tangent},
              {"snap_grid", sf.snap_grid},
              {"snap_grid_line", sf.snap_grid_line},
              {"snap_polar", sf.snap_polar},
              {"polar_angle_degrees", sf.polar_angle_degrees},
              {"magnetic_pull", sf.magnetic_pull},
              {"tolerance_px", sf.tolerance_px},
          };
        }()},
       {"snap_candidates",
        [&viewport]() {
          json arr = json::array();
          for (const auto& c : viewport.snap_candidates) {
            arr.push_back({
                {"kind", c.kind},
                {"entity_id", c.entity_id},
                {"point_id", c.point_id},
                {"local_x", c.local_x},
                {"local_y", c.local_y},
                {"label", c.label},
            });
          }
          return arr;
        }()},
       {"dof_statuses",
        [&viewport]() {
          json statuses = json::array();
          for (const auto& e : viewport.dof_statuses) {
            std::string status_str = "under";
            if (e.status == polysmith::core::DofStatus::FullyConstrained)
              status_str = "full";
            else if (e.status == polysmith::core::DofStatus::OverConstrained)
              status_str = "over";
            statuses.push_back({
                {"entity_id", e.entity_id},
                {"entity_kind", e.entity_kind},
                {"total_dof", e.total_dof},
                {"consumed_dof", e.consumed_dof},
                {"status", status_str},
            });
          }
          return statuses;
        }()},
  };
}

polysmith::core::FeatureEntry feature_entry_from_payload(const json& payload) {
  polysmith::core::FeatureEntry feature{};
  feature.id = read_string(payload, "feature_id");
  feature.kind = read_string(payload, "kind");
  feature.name = read_string(payload, "name");
  feature.status = read_string(payload, "status");
  feature.parameters_summary = read_string(payload, "parameters_summary");
  // Older `.polysmith` files predate the `suppressed` flag — treat
  // missing/non-bool fields as false so loading them is non-fatal.
  if (payload.contains("suppressed") && payload.at("suppressed").is_boolean()) {
    feature.suppressed = payload.at("suppressed").get<bool>();
  }
  // Older saves predate dependency tracking — default to clean state.
  // The first geometry mutation after load will recompute the flag
  // anyway via `refresh_history_dependencies`.
  if (payload.contains("dependency_broken") &&
      payload.at("dependency_broken").is_boolean()) {
    feature.dependency_broken = payload.at("dependency_broken").get<bool>();
  }
  if (payload.contains("dependency_warning") &&
      payload.at("dependency_warning").is_string()) {
    feature.dependency_warning =
        payload.at("dependency_warning").get<std::string>();
  }

  if (payload.contains("box_parameters") &&
      !payload.at("box_parameters").is_null()) {
    const json& box_payload = payload.at("box_parameters");
    polysmith::core::BoxFeatureParameters box{};
    box.width = read_number(box_payload, "width");
    box.height = read_number(box_payload, "height");
    box.depth = read_number(box_payload, "depth");
    feature.box_parameters = box;
  }

  if (payload.contains("cylinder_parameters") &&
      !payload.at("cylinder_parameters").is_null()) {
    const json& cyl_payload = payload.at("cylinder_parameters");
    polysmith::core::CylinderFeatureParameters cylinder{};
    cylinder.radius = read_number(cyl_payload, "radius");
    cylinder.height = read_number(cyl_payload, "height");
    feature.cylinder_parameters = cylinder;
  }

  if (payload.contains("extrude_parameters") &&
      !payload.at("extrude_parameters").is_null()) {
    feature.extrude_parameters =
        parse_extrude_parameters_from_payload(payload.at("extrude_parameters"));
  }

  if (payload.contains("loft_parameters") &&
      !payload.at("loft_parameters").is_null()) {
    feature.loft_parameters =
        loft_parameters_from_payload(payload.at("loft_parameters"));
  }

  if (payload.contains("revolve_parameters") &&
      !payload.at("revolve_parameters").is_null()) {
    feature.revolve_parameters =
        revolve_parameters_from_payload(payload.at("revolve_parameters"));
  }

  if (payload.contains("sweep_parameters") &&
      !payload.at("sweep_parameters").is_null()) {
    feature.sweep_parameters =
        sweep_parameters_from_payload(payload.at("sweep_parameters"));
  }

  if (payload.contains("sketch_parameters") &&
      !payload.at("sketch_parameters").is_null()) {
    feature.sketch_parameters =
        sketch_parameters_from_payload(payload.at("sketch_parameters"));
  }

  if (payload.contains("fillet_parameters") &&
      !payload.at("fillet_parameters").is_null()) {
    const json& fp = payload.at("fillet_parameters");
    polysmith::core::FilletFeatureParameters params{};
    params.target_body_id = read_string(fp, "target_body_id");
    if (fp.contains("edge_ids") && fp.at("edge_ids").is_array()) {
      for (const auto& edge : fp.at("edge_ids")) {
        params.edge_ids.push_back(edge.get<std::string>());
      }
    }
    params.radius = read_number(fp, "radius");
    // Default false when absent so older saved documents (created
    // before is_pending existed) load with the same semantics they
    // had at save time — i.e. "already confirmed".
    if (fp.contains("is_pending") && fp.at("is_pending").is_boolean()) {
      params.is_pending = fp.at("is_pending").get<bool>();
    }
    feature.fillet_parameters = params;
  }

  if (payload.contains("chamfer_parameters") &&
      !payload.at("chamfer_parameters").is_null()) {
    const json& cp = payload.at("chamfer_parameters");
    polysmith::core::ChamferFeatureParameters params{};
    params.target_body_id = read_string(cp, "target_body_id");
    if (cp.contains("edge_ids") && cp.at("edge_ids").is_array()) {
      for (const auto& edge : cp.at("edge_ids")) {
        params.edge_ids.push_back(edge.get<std::string>());
      }
    }
    params.distance = read_number(cp, "distance");
    if (cp.contains("is_pending") && cp.at("is_pending").is_boolean()) {
      params.is_pending = cp.at("is_pending").get<bool>();
    }
    feature.chamfer_parameters = params;
  }

  if (payload.contains("shell_parameters") &&
      !payload.at("shell_parameters").is_null()) {
    const json& sp = payload.at("shell_parameters");
    polysmith::core::ShellFeatureParameters params{};
    params.target_body_id = read_string(sp, "target_body_id");
    if (sp.contains("removed_face_ids") &&
        sp.at("removed_face_ids").is_array()) {
      for (const auto& face : sp.at("removed_face_ids")) {
        params.removed_face_ids.push_back(face.get<std::string>());
      }
    }
    params.thickness = read_number(sp, "thickness");
    if (sp.contains("is_pending") && sp.at("is_pending").is_boolean()) {
      params.is_pending = sp.at("is_pending").get<bool>();
    }
    feature.shell_parameters = params;
  }

  if (payload.contains("construction_plane_parameters") &&
      !payload.at("construction_plane_parameters").is_null()) {
    const json& cpp = payload.at("construction_plane_parameters");
    polysmith::core::ConstructionPlaneFeatureParameters params{};
    if (cpp.contains("plane_type") && cpp.at("plane_type").is_string()) {
      params.plane_type = cpp.at("plane_type").get<std::string>();
    }
    params.source_plane_id = read_string(cpp, "source_plane_id");
    if (cpp.contains("source_plane_ids") && cpp.at("source_plane_ids").is_array()) {
      for (const auto& id : cpp.at("source_plane_ids")) {
        if (id.is_string()) {
          params.source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (params.source_plane_ids.empty() && !params.source_plane_id.empty()) {
      params.source_plane_ids.push_back(params.source_plane_id);
    }
    if (cpp.contains("source_axis_id") && cpp.at("source_axis_id").is_string()) {
      params.source_axis_id = cpp.at("source_axis_id").get<std::string>();
    }
    params.offset = read_number(cpp, "offset");
    if (cpp.contains("angle_degrees") && cpp.at("angle_degrees").is_number()) {
      params.angle_degrees = cpp.at("angle_degrees").get<double>();
    }
    if (cpp.contains("plane_frame") && cpp.at("plane_frame").is_object()) {
      params.plane_frame = plane_frame_from_payload(cpp.at("plane_frame"));
    }
    feature.construction_plane_parameters = params;
  }

  if (payload.contains("construction_axis_parameters") &&
      !payload.at("construction_axis_parameters").is_null()) {
    const json& cap = payload.at("construction_axis_parameters");
    polysmith::core::ConstructionAxisFeatureParameters params{};
    params.source_id = read_string(cap, "source_id");
    params.source_kind =
        read_optional_string_value(cap, "source_kind", "edge");
    params.start_x = require(cap, "start").at("x").get<double>();
    params.start_y = require(cap, "start").at("y").get<double>();
    params.start_z = require(cap, "start").at("z").get<double>();
    params.end_x = require(cap, "end").at("x").get<double>();
    params.end_y = require(cap, "end").at("y").get<double>();
    params.end_z = require(cap, "end").at("z").get<double>();
    feature.construction_axis_parameters = params;
  }

  if (payload.contains("construction_point_parameters") &&
      !payload.at("construction_point_parameters").is_null()) {
    const json& cpp = payload.at("construction_point_parameters");
    polysmith::core::ConstructionPointFeatureParameters params{};
    params.source_id = read_string(cpp, "source_id");
    params.source_kind =
        read_optional_string_value(cpp, "source_kind", "vertex");
    params.x = require(cpp, "position").at("x").get<double>();
    params.y = require(cpp, "position").at("y").get<double>();
    params.z = require(cpp, "position").at("z").get<double>();
    feature.construction_point_parameters = params;
  }

  if (payload.contains("hole_parameters") &&
      !payload.at("hole_parameters").is_null()) {
    const json& hp = payload.at("hole_parameters");
    polysmith::core::HoleFeatureParameters params{};
    params.target_body_id = read_string(hp, "target_body_id");
    params.source_face_id = read_string(hp, "source_face_id");
    params.plane_frame = plane_frame_from_payload(require(hp, "plane_frame"));
    params.center_x = read_number(hp, "center_x");
    params.center_y = read_number(hp, "center_y");
    params.hole_type = read_optional_string_value(hp, "hole_type", "simple");
    params.extent_type = read_optional_string_value(hp, "extent_type", "blind");
    params.diameter = read_optional_number(hp, "diameter", params.diameter);
    params.depth = read_optional_number(hp, "depth", params.depth);
    params.counterbore_diameter = read_optional_number(
        hp, "counterbore_diameter", params.counterbore_diameter);
    params.counterbore_depth = read_optional_number(
        hp, "counterbore_depth", params.counterbore_depth);
    params.countersink_diameter = read_optional_number(
        hp, "countersink_diameter", params.countersink_diameter);
    params.countersink_angle_degrees = read_optional_number(
        hp, "countersink_angle_degrees", params.countersink_angle_degrees);
    params.standard =
        read_optional_string_value(hp, "standard", params.standard);
    params.standard_size =
        read_optional_string_value(hp, "standard_size", params.standard_size);
    params.hole_fit =
        read_optional_string_value(hp, "hole_fit", params.hole_fit);
    if (hp.contains("thread_enabled") && hp.at("thread_enabled").is_boolean()) {
      params.thread_enabled = hp.at("thread_enabled").get<bool>();
    }
    params.thread_spec =
        read_optional_string_value(hp, "thread_spec", params.thread_spec);
    params.thread_pitch =
        read_optional_number(hp, "thread_pitch", params.thread_pitch);
    params.major_diameter =
        read_optional_number(hp, "major_diameter", params.major_diameter);
    params.minor_diameter =
        read_optional_number(hp, "minor_diameter", params.minor_diameter);
    params.thread_depth =
        read_optional_number(hp, "thread_depth", params.thread_depth);
    params.thread_representation = read_optional_string_value(
        hp, "thread_representation", params.thread_representation);
    if (hp.contains("is_pending") && hp.at("is_pending").is_boolean()) {
      params.is_pending = hp.at("is_pending").get<bool>();
    }
    feature.hole_parameters = params;
  }

  if (payload.contains("helix_parameters") &&
      !payload.at("helix_parameters").is_null()) {
    const json& hp = payload.at("helix_parameters");
    polysmith::core::HelixFeatureParameters params{};
    params.axis_source_id = read_string(hp, "axis_source_id");
    params.axis_start_x = require(hp, "axis_start").at("x").get<double>();
    params.axis_start_y = require(hp, "axis_start").at("y").get<double>();
    params.axis_start_z = require(hp, "axis_start").at("z").get<double>();
    params.axis_end_x = require(hp, "axis_end").at("x").get<double>();
    params.axis_end_y = require(hp, "axis_end").at("y").get<double>();
    params.axis_end_z = require(hp, "axis_end").at("z").get<double>();
    params.radius = read_optional_number(hp, "radius", params.radius);
    params.pitch = read_optional_number(hp, "pitch", params.pitch);
    params.height = read_optional_number(hp, "height", params.height);
    params.turns = read_optional_number(hp, "turns", params.turns);
    params.handedness =
        read_optional_string_value(hp, "handedness", params.handedness);
    params.start_angle_degrees = read_optional_number(
        hp, "start_angle_degrees", params.start_angle_degrees);
    if (hp.contains("points") && hp.at("points").is_array()) {
      for (const auto& point_value : hp.at("points")) {
        if (point_value.is_number()) {
          params.points.push_back(point_value.get<double>());
        }
      }
    }
    feature.helix_parameters = params;
  }

  if (payload.contains("thread_parameters") &&
      !payload.at("thread_parameters").is_null()) {
    const json& tp = payload.at("thread_parameters");
    polysmith::core::ThreadFeatureParameters params{};
    params.target_body_id =
        read_optional_string_value(tp, "target_body_id", params.target_body_id);
    params.axis_source_id =
        read_optional_string_value(tp, "axis_source_id", params.axis_source_id);
    params.mode = read_optional_string_value(tp, "mode", params.mode);
    params.standard =
        read_optional_string_value(tp, "standard", params.standard);
    params.size = read_optional_string_value(tp, "size", params.size);
    params.major_diameter =
        read_optional_number(tp, "major_diameter", params.major_diameter);
    params.minor_diameter =
        read_optional_number(tp, "minor_diameter", params.minor_diameter);
    params.pitch = read_optional_number(tp, "pitch", params.pitch);
    params.length = read_optional_number(tp, "length", params.length);
    params.thread_angle_degrees = read_optional_number(
        tp, "thread_angle_degrees", params.thread_angle_degrees);
    params.start_offset =
        read_optional_number(tp, "start_offset", params.start_offset);
    params.handedness =
        read_optional_string_value(tp, "handedness", params.handedness);
    params.representation =
        read_optional_string_value(tp, "representation", params.representation);
    if (tp.contains("is_pending") && tp.at("is_pending").is_boolean()) {
      params.is_pending = tp.at("is_pending").get<bool>();
    }
    feature.thread_parameters = params;
  }

  if (payload.contains("fastener_parameters") &&
      !payload.at("fastener_parameters").is_null()) {
    const json& fp = payload.at("fastener_parameters");
    polysmith::core::FastenerFeatureParameters params{};
    params.standard =
        read_optional_string_value(fp, "standard", params.standard);
    params.size = read_optional_string_value(fp, "size", params.size);
    params.diameter = read_optional_number(fp, "diameter", params.diameter);
    params.minor_diameter =
        read_optional_number(fp, "minor_diameter", params.minor_diameter);
    params.pitch = read_optional_number(fp, "pitch", params.pitch);
    params.length = read_optional_number(fp, "length", params.length);
    params.thread_length =
        read_optional_number(fp, "thread_length", params.thread_length);
    params.head_type =
        read_optional_string_value(fp, "head_type", params.head_type);
    params.drive_type =
        read_optional_string_value(fp, "drive_type", params.drive_type);
    params.thread_representation = read_optional_string_value(
        fp, "thread_representation", params.thread_representation);
    feature.fastener_parameters = params;
  }

  if (payload.contains("move_parameters") &&
      !payload.at("move_parameters").is_null()) {
    const json& mp = payload.at("move_parameters");
    polysmith::core::MoveFeatureParameters params{};
    params.target_body_id =
        read_optional_string_value(mp, "target_body_id", params.target_body_id);
    params.translation_x =
        read_optional_number(mp, "translation_x", params.translation_x);
    params.translation_y =
        read_optional_number(mp, "translation_y", params.translation_y);
    params.translation_z =
        read_optional_number(mp, "translation_z", params.translation_z);
    params.rotation_x_degrees =
        read_optional_number(mp, "rotation_x_degrees", params.rotation_x_degrees);
    params.rotation_y_degrees =
        read_optional_number(mp, "rotation_y_degrees", params.rotation_y_degrees);
    params.rotation_z_degrees =
        read_optional_number(mp, "rotation_z_degrees", params.rotation_z_degrees);
    if (mp.contains("is_pending") && mp.at("is_pending").is_boolean()) {
      params.is_pending = mp.at("is_pending").get<bool>();
    }
    feature.move_parameters = params;
  }

  if (payload.contains("body_copy_parameters") &&
      !payload.at("body_copy_parameters").is_null()) {
    const json& bp = payload.at("body_copy_parameters");
    polysmith::core::BodyCopyFeatureParameters params{};
    params.source_body_id =
        read_optional_string_value(bp, "source_body_id", params.source_body_id);
    params.copy_mode =
        read_optional_string_value(bp, "copy_mode", params.copy_mode);
    params.source_body_name =
        read_optional_string_value(bp, "source_body_name", params.source_body_name);
    params.serialized_shape =
        read_optional_string_value(bp, "serialized_shape", params.serialized_shape);
    params.local_x_axis_x =
        read_optional_number(bp, "local_x_axis_x", params.local_x_axis_x);
    params.local_x_axis_y =
        read_optional_number(bp, "local_x_axis_y", params.local_x_axis_y);
    params.local_x_axis_z =
        read_optional_number(bp, "local_x_axis_z", params.local_x_axis_z);
    params.local_y_axis_x =
        read_optional_number(bp, "local_y_axis_x", params.local_y_axis_x);
    params.local_y_axis_y =
        read_optional_number(bp, "local_y_axis_y", params.local_y_axis_y);
    params.local_y_axis_z =
        read_optional_number(bp, "local_y_axis_z", params.local_y_axis_z);
    params.local_z_axis_x =
        read_optional_number(bp, "local_z_axis_x", params.local_z_axis_x);
    params.local_z_axis_y =
        read_optional_number(bp, "local_z_axis_y", params.local_z_axis_y);
    params.local_z_axis_z =
        read_optional_number(bp, "local_z_axis_z", params.local_z_axis_z);
    feature.body_copy_parameters = params;
  }

  return feature;
}

polysmith::core::DocumentState document_from_payload(const json& payload) {
  polysmith::core::DocumentState document{};
  document.id = read_string(payload, "document_id");
  document.name = read_string(payload, "name");
  document.units = read_string(payload, "units");
  document.revision = read_int(payload, "revision");
  document.selected_feature_id =
      read_optional_string(payload, "selected_feature_id");
  document.selected_reference_id =
      read_optional_string(payload, "selected_reference_id");
  document.selected_face_id = read_optional_string(payload, "selected_face_id");
  // Backward compat: pre-multi-select saves used a single
  // `selected_edge_id` (string|null). New saves use an array
  // `selected_edge_ids`. Read whichever is present so older `.polysmith`
  // files keep loading.
  if (payload.contains("selected_edge_ids") &&
      payload.at("selected_edge_ids").is_array()) {
    for (const auto& entry : payload.at("selected_edge_ids")) {
      if (entry.is_string()) {
        document.selected_edge_ids.push_back(entry.get<std::string>());
      }
    }
  } else {
    const auto legacy = read_optional_string(payload, "selected_edge_id");
    if (legacy.has_value()) {
      document.selected_edge_ids.push_back(*legacy);
    }
  }
  // Same back-compat shape as selected_edge_ids: read the array form
  // when present, fall back to the legacy single `selected_vertex_id`
  // for old `.polysmith` saves.
  if (payload.contains("selected_vertex_ids") &&
      payload.at("selected_vertex_ids").is_array()) {
    for (const auto& entry : payload.at("selected_vertex_ids")) {
      if (entry.is_string()) {
        document.selected_vertex_ids.push_back(entry.get<std::string>());
      }
    }
  } else {
    const auto legacy = read_optional_string(payload, "selected_vertex_id");
    if (legacy.has_value()) {
      document.selected_vertex_ids.push_back(*legacy);
    }
  }
  document.active_sketch_plane_id =
      read_optional_string(payload, "active_sketch_plane_id");
  document.active_sketch_face_id =
      read_optional_string(payload, "active_sketch_face_id");
  document.active_sketch_feature_id =
      read_optional_string(payload, "active_sketch_feature_id");
  document.active_sketch_tool =
      read_optional_string(payload, "active_sketch_tool");
  document.selected_sketch_point_id =
      read_optional_string(payload, "selected_sketch_point_id");
  document.selected_sketch_entity_id =
      read_optional_string(payload, "selected_sketch_entity_id");
  if (payload.contains("selected_sketch_point_ids") &&
      payload.at("selected_sketch_point_ids").is_array()) {
    for (const auto& id : payload.at("selected_sketch_point_ids")) {
      if (id.is_string()) {
        document.selected_sketch_point_ids.push_back(id.get<std::string>());
      }
    }
  }
  if (payload.contains("selected_sketch_entity_ids") &&
      payload.at("selected_sketch_entity_ids").is_array()) {
    for (const auto& id : payload.at("selected_sketch_entity_ids")) {
      if (id.is_string()) {
        document.selected_sketch_entity_ids.push_back(id.get<std::string>());
      }
    }
  }
  document.selected_sketch_dimension_id =
      read_optional_string(payload, "selected_sketch_dimension_id");
  document.selected_sketch_profile_id =
      read_optional_string(payload, "selected_sketch_profile_id");
  if (payload.contains("selected_sketch_profile_ids") &&
      payload.at("selected_sketch_profile_ids").is_array()) {
    for (const auto& entry : payload.at("selected_sketch_profile_ids")) {
      if (entry.is_string()) {
        document.selected_sketch_profile_ids.push_back(entry.get<std::string>());
      }
    }
  } else if (document.selected_sketch_profile_id.has_value()) {
    document.selected_sketch_profile_ids.push_back(
        document.selected_sketch_profile_id.value());
  }
  if (payload.contains("timeline_cursor") &&
      payload.at("timeline_cursor").is_number_integer()) {
    document.timeline_cursor = payload.at("timeline_cursor").get<int>();
  } else {
    document.timeline_cursor = std::nullopt;
  }

  if (payload.contains("feature_history") &&
      payload.at("feature_history").is_array()) {
    for (const auto& feature_payload : payload.at("feature_history")) {
      document.feature_history.push_back(
          feature_entry_from_payload(feature_payload));
    }
  }

  // Parameters — absent in older saves, default to empty
  if (payload.contains("parameters") &&
      payload.at("parameters").is_array()) {
    for (const auto& param_payload : payload.at("parameters")) {
      polysmith::core::ParameterEntry param;
      param.name = read_string(param_payload, "name");
      param.expression = read_string(param_payload, "expression");
      param.resolved_value = read_number(param_payload, "resolved_value");
      // kind default is "length" for backward compat with old saves
      if (param_payload.contains("kind") &&
          param_payload.at("kind").is_string()) {
        param.kind = param_payload.at("kind").get<std::string>();
      }
      param.has_error = read_bool(param_payload, "has_error");
      if (param_payload.contains("error_message") &&
          param_payload.at("error_message").is_string()) {
        param.error_message =
            param_payload.at("error_message").get<std::string>();
      }
      document.parameters.push_back(param);
    }
  }

  if (payload.contains("appearance") &&
      payload.at("appearance").is_object()) {
    const auto& appearance = payload.at("appearance");
    if (appearance.contains("body_colors") &&
        appearance.at("body_colors").is_array()) {
      for (const auto& entry_payload : appearance.at("body_colors")) {
        if (!entry_payload.is_object()) {
          continue;
        }
        polysmith::core::BodyAppearanceOverride entry;
        entry.body_id = read_string(entry_payload, "body_id");
        entry.color = read_string(entry_payload, "color");
        if (polysmith::core::is_valid_appearance_color(entry.color)) {
          document.appearance.body_colors.push_back(entry);
        }
      }
    }
    if (appearance.contains("face_colors") &&
        appearance.at("face_colors").is_array()) {
      for (const auto& entry_payload : appearance.at("face_colors")) {
        if (!entry_payload.is_object()) {
          continue;
        }
        polysmith::core::FaceAppearanceOverride entry;
        entry.face_id = read_string(entry_payload, "face_id");
        entry.owner_body_id = read_string(entry_payload, "owner_body_id");
        entry.signature = read_string(entry_payload, "signature");
        entry.color = read_string(entry_payload, "color");
        if (polysmith::core::is_valid_appearance_color(entry.color)) {
          document.appearance.face_colors.push_back(entry);
        }
      }
    }
  }

  // Read selection filter (v1 default if absent in old payloads)
  if (payload.contains("selection_filter") &&
      payload.at("selection_filter").is_object()) {
    const auto& sf = payload.at("selection_filter");
    auto& filter = document.selection_filter;
    if (sf.contains("select_curves") && sf.at("select_curves").is_boolean())
      filter.select_curves = sf.at("select_curves").get<bool>();
    if (sf.contains("select_points") && sf.at("select_points").is_boolean())
      filter.select_points = sf.at("select_points").get<bool>();
    if (sf.contains("select_construction") && sf.at("select_construction").is_boolean())
      filter.select_construction = sf.at("select_construction").get<bool>();
    if (sf.contains("select_constraints") && sf.at("select_constraints").is_boolean())
      filter.select_constraints = sf.at("select_constraints").get<bool>();
    if (sf.contains("snap_endpoint") && sf.at("snap_endpoint").is_boolean())
      filter.snap_endpoint = sf.at("snap_endpoint").get<bool>();
    if (sf.contains("snap_midpoint") && sf.at("snap_midpoint").is_boolean())
      filter.snap_midpoint = sf.at("snap_midpoint").get<bool>();
    if (sf.contains("snap_center") && sf.at("snap_center").is_boolean())
      filter.snap_center = sf.at("snap_center").get<bool>();
    if (sf.contains("snap_intersection") && sf.at("snap_intersection").is_boolean())
      filter.snap_intersection = sf.at("snap_intersection").get<bool>();
    if (sf.contains("snap_nearest") && sf.at("snap_nearest").is_boolean())
      filter.snap_nearest = sf.at("snap_nearest").get<bool>();
    if (sf.contains("snap_quadrant") && sf.at("snap_quadrant").is_boolean())
      filter.snap_quadrant = sf.at("snap_quadrant").get<bool>();
    if (sf.contains("snap_perpendicular") && sf.at("snap_perpendicular").is_boolean())
      filter.snap_perpendicular = sf.at("snap_perpendicular").get<bool>();
    if (sf.contains("snap_parallel") && sf.at("snap_parallel").is_boolean())
      filter.snap_parallel = sf.at("snap_parallel").get<bool>();
    if (sf.contains("snap_tangent") && sf.at("snap_tangent").is_boolean())
      filter.snap_tangent = sf.at("snap_tangent").get<bool>();
    if (sf.contains("snap_grid") && sf.at("snap_grid").is_boolean())
      filter.snap_grid = sf.at("snap_grid").get<bool>();
    if (sf.contains("snap_grid_line") && sf.at("snap_grid_line").is_boolean())
      filter.snap_grid_line = sf.at("snap_grid_line").get<bool>();
    if (sf.contains("snap_polar") && sf.at("snap_polar").is_boolean())
      filter.snap_polar = sf.at("snap_polar").get<bool>();
    if (sf.contains("polar_angle_degrees") && sf.at("polar_angle_degrees").is_number())
      filter.polar_angle_degrees = sf.at("polar_angle_degrees").get<int>();
    if (sf.contains("magnetic_pull") && sf.at("magnetic_pull").is_boolean())
      filter.magnetic_pull = sf.at("magnetic_pull").get<bool>();
    if (sf.contains("tolerance_px") && sf.at("tolerance_px").is_number())
      filter.tolerance_px = sf.at("tolerance_px").get<int>();
  }

  // CAM setup (absent in older documents — stays nullopt)
  if (payload.contains("cam_setup") && payload.at("cam_setup").is_object()) {
    document.cam_setup = setup_from_payload(payload.at("cam_setup"));
  }

  // CAM tool library (absent in older documents — stays empty)
  if (payload.contains("tool_library") && payload.at("tool_library").is_array()) {
    for (const auto& tool_json : payload.at("tool_library")) {
      if (!tool_json.is_object()) continue;
      polysmith::core::CamToolDefinition tool;
      if (tool_json.contains("tool_id")) tool.id = tool_json["tool_id"].get<std::string>();
      if (tool_json.contains("name")) tool.name = tool_json["name"].get<std::string>();
      if (tool_json.contains("tool_number")) tool.toolNumber = tool_json["tool_number"].get<int>();
      if (tool_json.contains("diameter")) tool.diameter = tool_json["diameter"].get<double>();
      if (tool_json.contains("flute_length")) tool.fluteLength = tool_json["flute_length"].get<double>();
      if (tool_json.contains("overall_length")) tool.overallLength = tool_json["overall_length"].get<double>();
      if (tool_json.contains("corner_radius")) tool.cornerRadius = tool_json["corner_radius"].get<double>();
      if (tool_json.contains("spindle_speed")) tool.spindleSpeed = tool_json["spindle_speed"].get<double>();
      if (tool_json.contains("feed_rate")) tool.feedRate = tool_json["feed_rate"].get<double>();
      if (tool_json.contains("stepover")) tool.stepover = tool_json["stepover"].get<double>();
      if (tool_json.contains("stepdown")) tool.stepdown = tool_json["stepdown"].get<double>();
      if (tool_json.contains("type")) {
        tool.type = static_cast<polysmith::core::CamToolDefinition::Type>(tool_json["type"].get<int>());
      }
      document.tool_library.push_back(tool);
    }
  }

  return document;
}

}  // namespace polysmith::protocol
