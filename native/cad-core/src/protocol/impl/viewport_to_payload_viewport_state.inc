json to_payload(const polysmith::core::ViewportState& viewport) {
  return to_payload(viewport, {}, 0);
}

json to_payload(
    const polysmith::core::ViewportState& viewport,
    const std::map<std::string, std::string>& known_mesh_revisions,
    int known_revision) {
  // The client already holds this exact scene: send only what selection changes.
  const bool scene_unchanged =
      known_revision != 0 && known_revision == viewport.revision;
#include "protocol/impl/viewport_to_payload_state_array_collectors.inc"
  return {
#include "protocol/impl/viewport_to_payload_state_basic_entries.inc"
#include "protocol/impl/viewport_to_payload_state_cam_entries.inc"
#include "protocol/impl/viewport_to_payload_state_body_topology_entries.inc"
#include "protocol/impl/viewport_to_payload_state_scene_interaction_entries.inc"
  };
}
