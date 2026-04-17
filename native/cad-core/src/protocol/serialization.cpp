#include "protocol/serialization.h"

namespace polysmith::protocol {

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

}  // namespace polysmith::protocol
