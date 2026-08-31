#include "core/cam/face_milling.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_planning.h"
#include "core/diagnostics/logger.h"
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
using cam2d::xy_length;
using cam2d::xy_signed_area;

#include "core/cam/impl/face_milling_generate.inc"

}  // namespace

void register_face_milling_generator() {
  register_cam_generator({"face_milling", generate_face_milling_toolpath,
                          generate_face_milling_toolpath});
}

}  // namespace polysmith::core
