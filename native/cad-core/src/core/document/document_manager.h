#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_operation.h"
#include "core/document/document_state.h"
#include "core/export/export.h"
#include "core/extrude/extrude_feature.h"
#include "core/loft/loft_feature.h"
#include "core/plugin/plugin_feature.h"
#include "core/primitive/box_feature.h"
#include "core/primitive/cylinder_feature.h"
#include "core/revolve/revolve_feature.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_profile.h"
#include "core/sweep/sweep_feature.h"

namespace polysmith::core {

class DocumentManager {
 public:
#include "core/document/impl/document_manager_document_commands.inc"
#include "core/document/impl/document_manager_profile_feature_commands.inc"
#include "core/document/impl/document_manager_selection_commands.inc"
#include "core/document/impl/document_manager_body_modifier_commands.inc"
#include "core/document/impl/document_manager_construction_commands.inc"
#include "core/document/impl/document_manager_sketch_session_commands.inc"
#include "core/document/impl/document_manager_sketch_constraint_commands.inc"
#include "core/document/impl/document_manager_sketch_dimension_commands.inc"
#include "core/document/impl/document_manager_sketch_entity_commands.inc"
#include "core/document/impl/document_manager_projection_parameter_commands.inc"
#include "core/document/impl/document_manager_io_commands.inc"
#include "core/document/impl/document_manager_cam_commands.inc"
#include "core/document/impl/document_manager_dxf_commands.inc"

 private:
#include "core/document/impl/document_manager_private_state.inc"
};

}  // namespace polysmith::core
