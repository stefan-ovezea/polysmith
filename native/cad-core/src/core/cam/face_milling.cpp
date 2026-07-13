// face_milling.cpp — stubbed while CAM workspace is rebuilt on
// the new cam_types.h schema.  Toolpath generation will be
// re-implemented when the CAM operations system is rewritten.

#include "core/cam/cam_operation.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

namespace polysmith::core {

// Face milling geometry helpers (needed by toolpath generation).
#include "core/cam/impl/face_milling_geometry_helpers.inc"

}  // namespace polysmith::core
