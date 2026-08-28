#pragma once

namespace polysmith::core {

// Registers the face_milling generator with the CAM generator registry.
// Called from register_builtin_cam_generators() after OCCT init.
void register_face_milling_generator();

}  // namespace polysmith::core
