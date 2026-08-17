polysmith::core::SketchFeatureParameters
sketch_parameters_from_payload(const json& payload) {
  polysmith::core::SketchFeatureParameters params{};
  params.plane_id = read_string(payload, "plane_id");
  // Restore the vertex-id counter so regenerated vertices (centers,
  // quadrants, trim split points) never collide with restored entity
  // ids — a collision would silently adopt another vertex's coordinates
  // (user-reported: a circle's center moved onto a projected corner
  // point after save/reload).
  if (payload.contains("next_vertex_index") &&
      payload.at("next_vertex_index").is_number_integer()) {
    params.next_vertex_index = payload.at("next_vertex_index").get<int>();
  }
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = sketch_plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.active_tool = payload.contains("active_tool") &&
                               payload.at("active_tool").is_string()
                           ? payload.at("active_tool").get<std::string>()
                           : std::string{};
#include "protocol/impl/feature_parameter_sketch_geometry_parser.inc"
#include "protocol/impl/feature_parameter_sketch_dimension_parser.inc"
#include "protocol/impl/feature_parameter_sketch_relation_parser.inc"
#include "protocol/impl/feature_parameter_sketch_profile_parser.inc"
#include "protocol/impl/feature_parameter_sketch_pending_mirror_parser.inc"
  return params;
}
