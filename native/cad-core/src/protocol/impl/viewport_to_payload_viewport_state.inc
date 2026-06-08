json to_payload(const polysmith::core::ViewportState& viewport) {
#include "protocol/impl/viewport_to_payload_state_array_collectors.inc"
  return {
#include "protocol/impl/viewport_to_payload_state_basic_entries.inc"
#include "protocol/impl/viewport_to_payload_state_cam_entries.inc"
#include "protocol/impl/viewport_to_payload_state_body_topology_entries.inc"
#include "protocol/impl/viewport_to_payload_state_scene_interaction_entries.inc"
  };
}
