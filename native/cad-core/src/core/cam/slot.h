#pragma once

namespace polysmith::core {

// Registers the slot generator with the CAM generator registry.
// Called from register_builtin_cam_generators() after OCCT init.
void register_slot_generator();

}  // namespace polysmith::core
