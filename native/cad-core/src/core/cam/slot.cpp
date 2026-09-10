#include "core/cam/slot.h"

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

#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace polysmith::core {

namespace {

// The generator .inc file keeps compiling against the shared 2D types
// unqualified.
using cam2d::XY;
using cam2d::xy_length;

#include "core/cam/impl/slot_generate.inc"

}  // namespace

void register_slot_generator() {
  register_cam_generator(
      {"slot", generate_slot_toolpath, generate_slot_toolpath});
}

}  // namespace polysmith::core
