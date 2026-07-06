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
json to_payload(const polysmith::core::ViewportToolpathPrimitive& primitive);
json to_payload(const polysmith::core::ViewportState& viewport);

// ── CAM types (cam_types.h) ─────────────────────────────────────

json to_payload(const polysmith::core::Bounds3D& b);
json to_payload(const polysmith::core::FaceAttestation& att);
json to_payload(const polysmith::core::EdgeAttestation& att);
json to_payload(const polysmith::core::GeometryReference& ref);
json to_payload(const polysmith::core::StockDefinition& s);
json to_payload(const polysmith::core::MachineAxes& ma);
json to_payload(const polysmith::core::WcsOrigin& wcs);
json to_payload(const polysmith::core::CamSetup& setup);
json to_payload(const polysmith::core::ToolEntry& tool);
json to_payload(const polysmith::core::CamGeometryReferences& geo);
json to_payload(const polysmith::core::CamPointLocation& loc);
json to_payload(const polysmith::core::CamOperationParameters& p);
json to_payload(const polysmith::core::CamOperationDependencies& d);
json to_payload(const polysmith::core::ExternalStorage& es);
json to_payload(const polysmith::core::ToolpathCache& tc);
json to_payload(const polysmith::core::CamOperation& op);
json to_payload(const polysmith::core::PostProcessorOptions& opt);
json to_payload(const polysmith::core::PostProcessor& pp);
json to_payload(const polysmith::core::CollisionReport& cr);
json to_payload(const polysmith::core::SimulationData& sim);
json to_payload(const polysmith::core::CamDocumentData& cam);

polysmith::core::ExtrudeFeatureParameters extrude_parameters_from_payload(
    const json& payload);

// Inverse of `to_payload(DocumentState)`. Used when loading a saved
// `.polysmith` document back into the core. Throws std::runtime_error on
// malformed payloads.
polysmith::core::FeatureEntry feature_entry_from_payload(const json& payload);
polysmith::core::DocumentState document_from_payload(const json& payload);

}  // namespace polysmith::protocol
