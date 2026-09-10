#include "core/cam/drilling_generator.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <Bnd_Box.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_Plane.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pln.hxx>

#include "core/cam/cam_generator.h"
#include "core/cam/cam_planning.h"
#include "core/cam/cam_stock.h"
#include "core/cam/milling_common.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

namespace {

#include "core/cam/impl/drilling_generate.inc"

}  // namespace

void register_drilling_generator() {
  register_cam_generator({"drilling", generate_drilling_toolpath,
                          generate_drilling_toolpath});
}

}  // namespace polysmith::core
