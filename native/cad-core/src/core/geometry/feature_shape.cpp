#include "core/geometry/feature_shape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <Geom_Plane.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeSegment.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>

#include "core/plugin/plugin_feature.h"

namespace polysmith::core {
namespace {

#include "core/geometry/impl/shape_frame_helpers.inc"
#include "core/geometry/impl/profile_face_helpers.inc"
#include "core/geometry/impl/sketch_wire_extrude.inc"
#include "core/geometry/impl/extrude_shape_helpers.inc"
}  // namespace

#include "core/geometry/impl/primitive_extrude_shapes.inc"
#include "core/geometry/impl/plugin_geometry_shapes.inc"
#include "core/geometry/impl/loft_revolve_sweep_hole_shapes.inc"
#include "core/geometry/impl/fastener_shapes.inc"
#include "core/geometry/impl/feature_shape_dispatch.inc"
}  // namespace polysmith::core
