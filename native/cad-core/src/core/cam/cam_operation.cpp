#include "core/cam/cam_operation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {
namespace {

#include "core/cam/impl/cam_face_reference_helpers.inc"

}  // namespace

#include "core/cam/impl/cam_face_reference_capture.inc"
#include "core/cam/impl/cam_face_reference_resolve.inc"
#include "core/cam/impl/cam_edge_reference_capture.inc"
#include "core/cam/impl/cam_edge_reference_resolve.inc"

}  // namespace polysmith::core
