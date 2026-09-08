#include "core/cam/contour_2d.h"

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
#include "core/geometry/body_compiler.h"

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace polysmith::core {

namespace {

// The generator .inc file keeps compiling against the shared 2D types
// unqualified.
using cam2d::BaseSegment;
using cam2d::OffsetSegment;
using cam2d::XY;
using cam2d::base_segments_signed_area;
using cam2d::kOffsetEps;
using cam2d::offset_closed_loop;
using cam2d::offset_loop_self_intersects;
using cam2d::reverse_segments;
using cam2d::sample_offset_loop;
using cam2d::xy_signed_area;

#include "core/cam/impl/contour_2d_generate.inc"

}  // namespace

void register_contour_2d_generator() {
  register_cam_generator({"contour_2d", generate_contour_2d_toolpath,
                          generate_contour_2d_toolpath});
}

}  // namespace polysmith::core
