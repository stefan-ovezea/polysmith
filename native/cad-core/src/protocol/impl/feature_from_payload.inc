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
#include "protocol/impl/feature_from_payload_primitive_blocks.inc"
#include "protocol/impl/feature_from_payload_profile_feature_blocks.inc"
#include "protocol/impl/feature_from_payload_body_modifier_blocks.inc"
#include "protocol/impl/feature_from_payload_construction_blocks.inc"
#include "protocol/impl/feature_from_payload_threaded_blocks.inc"
#include "protocol/impl/feature_from_payload_transform_blocks.inc"
  return feature;
}
