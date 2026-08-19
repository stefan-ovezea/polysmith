#include "core/geometry/refresh_dependents.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
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
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/construction/construction_plane_feature.h"
#include "core/document/document.h"
#include "core/geometry/edge_geometry.h"
#include "core/geometry/face_geometry.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/mesh_import_helpers.h"
#include "core/geometry/mesh_projection.h"
#include "core/document/feature.h"
#include "core/sketch/sketch_feature.h"

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
