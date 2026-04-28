#include "protocol/serialization.h"

#include <stdexcept>

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

polysmith::core::ExtrudeFeatureParameters
extrude_parameters_from_payload(const json& payload) {
  polysmith::core::ExtrudeFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
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
  params.depth = read_number(payload, "depth");
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
      params.circles.push_back(circle);
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
      dimension.value = read_number(dim_payload, "value");
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
      profile.source_circle_id =
          read_optional_string(profile_payload, "source_circle_id");
      profile.center_x = read_number(profile_payload, "center_x");
      profile.center_y = read_number(profile_payload, "center_y");
      profile.radius = read_number(profile_payload, "radius");
      params.profiles.push_back(profile);
    }
  }
  return params;
}

}  // namespace

json to_payload(const polysmith::core::FeatureEntry& feature) {
  return {
      {"feature_id", feature.id},
      {"kind", feature.kind},
      {"name", feature.name},
      {"status", feature.status},
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
                 {"depth", feature.extrude_parameters->depth},
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
                      });
                    }
                    return circles;
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
                          {"value", dimension.value},
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

                      profiles.push_back({
                          {"profile_id", profile.id},
                          {"kind", profile.kind},
                          {"point_ids", point_ids},
                          {"line_ids", line_ids},
                          {"points", points},
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
      {"selected_sketch_dimension_id",
       document.selected_sketch_dimension_id.has_value()
           ? json(document.selected_sketch_dimension_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_id",
       document.selected_sketch_profile_id.has_value()
           ? json(document.selected_sketch_profile_id.value())
           : json(nullptr)},
      {"feature_history", feature_history},
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
      {"is_selected", face.is_selected},
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
  };
}

json to_payload(const polysmith::core::ViewportSketchCirclePrimitive& primitive) {
  return {
      {"circle_id", primitive.circle_id},
      {"plane_id", primitive.plane_id},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
       }},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
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
      {"start_x", primitive.start_x},
      {"start_y", primitive.start_y},
      {"width", primitive.width},
      {"height", primitive.height},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
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

  json sketch_lines = json::array();
  for (const auto& line : viewport.sketch_lines) {
    sketch_lines.push_back(to_payload(line));
  }

  json sketch_circles = json::array();
  for (const auto& circle : viewport.sketch_circles) {
    sketch_circles.push_back(to_payload(circle));
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

  return {
      {"has_active_document", viewport.has_active_document},
      {"boxes", boxes},
      {"cylinders", cylinders},
      {"polygon_extrudes", polygon_extrudes},
      {"solid_faces", solid_faces},
      {"reference_planes", reference_planes},
      {"reference_axes", reference_axes},
      {"sketch_lines", sketch_lines},
      {"sketch_circles", sketch_circles},
      {"sketch_points", sketch_points},
      {"sketch_dimensions", sketch_dimensions},
      {"sketch_constraints", sketch_constraints},
      {"sketch_profiles", sketch_profiles},
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
  };
}

polysmith::core::FeatureEntry feature_entry_from_payload(const json& payload) {
  polysmith::core::FeatureEntry feature{};
  feature.id = read_string(payload, "feature_id");
  feature.kind = read_string(payload, "kind");
  feature.name = read_string(payload, "name");
  feature.status = read_string(payload, "status");
  feature.parameters_summary = read_string(payload, "parameters_summary");

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
        extrude_parameters_from_payload(payload.at("extrude_parameters"));
  }

  if (payload.contains("sketch_parameters") &&
      !payload.at("sketch_parameters").is_null()) {
    feature.sketch_parameters =
        sketch_parameters_from_payload(payload.at("sketch_parameters"));
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
  document.selected_sketch_dimension_id =
      read_optional_string(payload, "selected_sketch_dimension_id");
  document.selected_sketch_profile_id =
      read_optional_string(payload, "selected_sketch_profile_id");

  if (payload.contains("feature_history") &&
      payload.at("feature_history").is_array()) {
    for (const auto& feature_payload : payload.at("feature_history")) {
      document.feature_history.push_back(
          feature_entry_from_payload(feature_payload));
    }
  }

  return document;
}

}  // namespace polysmith::protocol
