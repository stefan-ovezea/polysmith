#include "core/document.h"
#include "core/sketch_feature.h"

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
#include <BRepAlgoAPI_Common.hxx>
#include <BRep_Builder.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/body_compiler.h"
#include "core/construction_plane_feature.h"
#include "core/edge_geometry.h"
#include "core/face_geometry.h"
#include "core/feature_shape.h"
#include "core/formula_eval.h"
#include "core/refresh_dependents.h"
#include "protocol/serialization.h"

namespace polysmith::core {
namespace {

#include "core/document/impl/private_document_helpers.inc"
#include "core/document/impl/private_extrude_extent_helpers.inc"
#include "core/document/impl/private_profile_feature_helpers.inc"
#include "core/document/impl/private_linked_feature_refresh.inc"
#include "core/document/impl/private_selection_projection_helpers.inc"
}  // namespace

#include "core/document/impl/lifecycle.inc"
#include "core/document/impl/selection.inc"
#include "core/document/impl/edge_features.inc"
#include "core/document/impl/sketch_commands.inc"
#include "core/document/impl/solid_feature_commands.inc"
#include "core/document/impl/sketch_entity_commands.inc"
#include "core/document/impl/projection_and_construction_commands.inc"
#include "core/document/impl/body_appearance_commands.inc"
#include "core/document/impl/document_io_commands.inc"
#include "core/document/impl/session_cam_commands.inc"
}  // namespace polysmith::core
