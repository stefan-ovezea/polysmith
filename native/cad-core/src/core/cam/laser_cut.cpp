#include "core/cam/laser_cut.h"

#include "core/cam/cam_generator.h"
#include "core/cam/laser/laser_generate.h"

namespace polysmith::core {

void register_laser_cut_generator() {
  register_cam_generator(
      {"laser_cut", laser::generate_laser_cut_toolpath,
       laser::generate_laser_cut_toolpath});
}

}  // namespace polysmith::core
