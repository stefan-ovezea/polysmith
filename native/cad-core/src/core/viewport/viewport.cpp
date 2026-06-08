#include "core/viewport/viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <GProp_GProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/appearance.h"
#include "core/sketch/dof_counter.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/refresh_dependents.h"
#include "core/sketch/sketch_profile.h"

namespace polysmith::core {
namespace {

#include "core/viewport/impl/common_helpers.inc"
#include "core/viewport/impl/sketch_primitives.inc"
#include "core/viewport/impl/body_shape_helpers.inc"

}  // namespace

ViewportState build_viewport_state(const std::optional<DocumentState>& document) {
#include "core/viewport/impl/empty_viewport_state.inc"
#include "core/viewport/impl/viewport_working_sets.inc"
#include "core/viewport/impl/cut_preview_emit.inc"
#include "core/viewport/impl/body_summary_emit.inc"

#include "core/viewport/impl/feature_history_emit.inc"

#include "core/viewport/impl/legacy_face_cleanup.inc"
#include "core/viewport/impl/scene_bounds_emit.inc"
#include "core/viewport/impl/snap_candidate_emit.inc"
#include "core/viewport/impl/dof_status_emit.inc"
#include "core/viewport/impl/face_appearance_finalize.inc"
#include "core/viewport/impl/cam_toolpath_emit.inc"
#include "core/viewport/impl/viewport_state_return.inc"
}

}  // namespace polysmith::core
