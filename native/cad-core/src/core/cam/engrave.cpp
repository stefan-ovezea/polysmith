#include "core/cam/engrave.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_planning.h"
#include "core/cam/cam_stock.h"
#include "core/cam/milling_common.h"

namespace polysmith::core {

namespace {

// The generator .inc file keeps compiling against the shared 2D types
// unqualified.
using cam2d::BaseSegment;
using cam2d::XY;
using cam2d::base_segments_signed_area;
using cam2d::reverse_segments;
using cam2d::xy_length;

#include "core/cam/impl/engrave_generate.inc"

}  // namespace

void register_engrave_generator() {
  register_cam_generator(
      {"engrave", generate_engrave_toolpath, generate_engrave_toolpath});
}

}  // namespace polysmith::core
