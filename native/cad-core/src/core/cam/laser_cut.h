#pragma once

namespace polysmith::core {

// Registers the laser_cut generator with the CAM generator registry.
// Called from register_builtin_cam_generators() after OCCT init.
void register_laser_cut_generator();

}  // namespace polysmith::core
