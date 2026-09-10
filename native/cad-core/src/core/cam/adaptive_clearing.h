#pragma once

namespace polysmith::core {

// Registers the adaptive_clearing generator with the CAM generator
// registry.  Called from register_builtin_cam_generators() after OCCT
// init.
void register_adaptive_clearing_generator();

}  // namespace polysmith::core
