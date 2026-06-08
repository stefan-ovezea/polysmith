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
#include "protocol/impl/feature_parameter_sketch_geometry_parser.inc"
#include "protocol/impl/feature_parameter_sketch_dimension_parser.inc"
#include "protocol/impl/feature_parameter_sketch_relation_parser.inc"
#include "protocol/impl/feature_parameter_sketch_profile_parser.inc"
#include "protocol/impl/feature_parameter_sketch_pending_mirror_parser.inc"
  return params;
}
