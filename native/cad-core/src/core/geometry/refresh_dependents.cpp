#include "core/refresh_dependents.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp_Face.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/body_compiler.h"
#include "core/construction_plane_feature.h"
#include "core/document.h"
#include "core/edge_geometry.h"
#include "core/face_geometry.h"
#include "core/feature.h"
#include "core/sketch_feature.h"

namespace polysmith::core {
namespace {

#include "core/geometry/impl/dependency_frame_helpers.inc"
}  // namespace

#include "core/geometry/impl/construction_source_resolvers.inc"
namespace {

#include "core/geometry/impl/projection_refresh_helpers.inc"
}  // namespace

void refresh_history_dependencies(DocumentState& document) {
#include "core/geometry/impl/refresh_history_body.inc"
}

}  // namespace polysmith::core
