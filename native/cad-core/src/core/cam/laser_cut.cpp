#include "core/cam/laser_cut.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "core/cam/cam_generator.h"
#include "core/diagnostics/logger.h"
#include "core/geometry/body_compiler.h"
#include "core/sketch/sketch_feature_parameters.h"
#include "core/sketch/sketch_profile_types.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>

namespace polysmith::core {

namespace {

#include "core/cam/impl/polygon_offset_helpers.inc"
#include "core/cam/impl/offset_loop.inc"
#include "core/cam/impl/laser_cut_offset.inc"
#include "core/cam/impl/laser_cut_generate.inc"

}  // namespace

void register_laser_cut_generator() {
  register_cam_generator(
      {"laser_cut", generate_laser_cut_toolpath, generate_laser_cut_toolpath});
}

}  // namespace polysmith::core
