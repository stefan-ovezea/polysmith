#include "core/cam/face_milling.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "core/cam/cam_generator.h"
#include "core/diagnostics/logger.h"
#include "core/geometry/body_compiler.h"

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
#include <gp_Vec.hxx>

namespace polysmith::core {

namespace {

#include "core/cam/impl/polygon_offset_helpers.inc"
#include "core/cam/impl/offset_loop.inc"
#include "core/cam/impl/face_milling_generate.inc"

}  // namespace

void register_face_milling_generator() {
  register_cam_generator({"face_milling", generate_face_milling_toolpath,
                          generate_face_milling_toolpath});
}

}  // namespace polysmith::core
