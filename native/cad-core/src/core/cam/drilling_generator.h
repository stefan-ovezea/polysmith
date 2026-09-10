#pragma once

namespace polysmith::core {

// Registers the drilling generator with the CAM generator registry.
// Called from register_builtin_cam_generators() after OCCT init.
void register_drilling_generator();

}  // namespace polysmith::core
