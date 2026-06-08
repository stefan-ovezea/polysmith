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
#include "protocol/impl/feature_to_payload_primitive_entries.inc"
#include "protocol/impl/feature_to_payload_extrude_entries.inc"
#include "protocol/impl/feature_to_payload_profile_feature_entries.inc"
#include "protocol/impl/feature_to_payload_sketch_entries.inc"
#include "protocol/impl/feature_to_payload_body_modifier_entries.inc"
#include "protocol/impl/feature_to_payload_construction_entries.inc"
#include "protocol/impl/feature_to_payload_threaded_entries.inc"
#include "protocol/impl/feature_to_payload_transform_entries.inc"
  };
}
