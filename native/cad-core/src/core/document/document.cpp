#include "core/document/document.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_profile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRep_Builder.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/geometry/body_compiler.h"
#include "core/construction/construction_plane_feature.h"
#include "core/geometry/edge_geometry.h"
#include "core/geometry/face_geometry.h"
#include "core/geometry/feature_shape.h"
#include "core/sketch/formula_eval.h"
#include "core/sketch/impl/private_vertex_lookup_helpers.inc"
#include "core/geometry/refresh_dependents.h"
#include "protocol/serialization.h"

namespace polysmith::core {
namespace {

#include "core/document/impl/private_document_helpers.inc"
#include "core/document/impl/private_extrude_extent_helpers.inc"
#include "core/document/impl/private_profile_feature_helpers.inc"
#include "core/document/impl/private_sketch_feature_parameter_helpers.inc"
#include "core/document/impl/private_linked_feature_refresh.inc"
#include "core/document/impl/private_selection_projection_helpers.inc"
#include "core/document/impl/private_projection_command_helpers.inc"
#include "core/document/impl/private_threaded_feature_helpers.inc"
}  // namespace

#include "core/document/impl/lifecycle.inc"
#include "core/document/impl/selection.inc"
#include "core/document/impl/edge_features.inc"
#include "core/document/impl/sketch_commands.inc"
#include "core/document/impl/sketch_dimension_commands.inc"
#include "core/document/impl/sketch_trim_commands.inc"
#include "core/document/impl/profile_selection_commands.inc"
#include "core/document/impl/extrude_commands.inc"
#include "core/document/impl/loft_commands.inc"
#include "core/document/impl/revolve_commands.inc"
#include "core/document/impl/sweep_commands.inc"
#include "core/document/impl/sketch_entity_commands.inc"
#include "core/document/impl/sketch_selection_commands.inc"
#include "core/document/impl/projection_commands.inc"
#include "core/document/impl/construction_commands.inc"
#include "core/document/impl/hole_commands.inc"
#include "core/document/impl/body_appearance_commands.inc"
#include "core/document/impl/document_io_commands.inc"
#include "core/document/impl/session_cam_commands.inc"
}  // namespace polysmith::core
