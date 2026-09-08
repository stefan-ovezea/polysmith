#include "core/cam/pocket_2d.h"

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

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
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
using cam2d::clip_segment_outside_polygon;
using cam2d::clip_segment_to_polygon;
using cam2d::kPi;
using cam2d::offset_closed_loop;
using cam2d::offset_loop_self_intersects;
using cam2d::reverse_segments;
using cam2d::sample_offset_loop;
using cam2d::xy_area_centroid;
using cam2d::xy_length;
using cam2d::xy_point_in_polygon;
using cam2d::xy_point_segment_distance;
using cam2d::xy_signed_area;

#include "core/cam/impl/pocket_2d_generate.inc"

}  // namespace

void register_pocket_2d_generator() {
  register_cam_generator({"pocket_2d", generate_pocket_2d_toolpath,
                          generate_pocket_2d_toolpath});
}

}  // namespace polysmith::core
