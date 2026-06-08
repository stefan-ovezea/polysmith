#pragma once

#include <nlohmann/json.hpp>

#include "core/cam/cam_operation.h"
#include "core/document/document.h"
#include "core/viewport/viewport.h"

namespace polysmith::protocol {

using json = nlohmann::json;

json to_payload(const polysmith::core::FeatureEntry& feature);
json to_payload(const polysmith::core::DocumentState& document);
json to_payload(const polysmith::core::SessionState& session);
json to_payload(const polysmith::core::ViewportBoxPrimitive& primitive);
json to_payload(const polysmith::core::ViewportCylinderPrimitive& primitive);
json to_payload(const polysmith::core::ViewportPolygonExtrudePrimitive& primitive);
json to_payload(const polysmith::core::ViewportSolidFace& face);
json to_payload(const polysmith::core::ViewportSketchLinePrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchCirclePrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchArcPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchPointPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchDimensionPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchConstraintPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchProfilePrimitive& primitive);
json to_payload(const polysmith::core::CamStockDefinition& stock);
json to_payload(const polysmith::core::CamSetup& setup);
json to_payload(const polysmith::core::CamToolDefinition& tool);
json to_payload(const polysmith::core::ViewportToolpathPrimitive& primitive);
json to_payload(const polysmith::core::ViewportState& viewport);

polysmith::core::CamStockDefinition stock_from_payload(const json& payload);
polysmith::core::CamSetup setup_from_payload(const json& payload);
polysmith::core::CamToolDefinition tool_from_payload(const json& payload);

polysmith::core::ExtrudeFeatureParameters extrude_parameters_from_payload(
    const json& payload);

// Inverse of `to_payload(DocumentState)`. Used when loading a saved
// `.polysmith` document back into the core. Throws std::runtime_error on
// malformed payloads.
polysmith::core::FeatureEntry feature_entry_from_payload(const json& payload);
polysmith::core::DocumentState document_from_payload(const json& payload);

}  // namespace polysmith::protocol
