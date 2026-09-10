#pragma once

namespace polysmith::core {

// Registers the pocket_2d generator with the CAM generator registry.
// Called from register_builtin_cam_generators() after OCCT init.
void register_pocket_2d_generator();

}  // namespace polysmith::core
