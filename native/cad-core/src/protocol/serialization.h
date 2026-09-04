#pragma once

#include <nlohmann/json.hpp>

#include "core/cam/cam_operation.h"
#include "core/document/document.h"
#include "core/viewport/viewport.h"

namespace polysmith::protocol {

using json = nlohmann::json;

// When `include_opaque` is false (the default, used for document_state
// events), large opaque blobs the UI never reads — B-rep shape
// snapshots on mesh_to_body and standalone body copies — are omitted
// from the payload. Save/load passes true to persist them.
json to_payload(const polysmith::core::FeatureEntry& feature,
                bool include_opaque = false);
json to_payload(const polysmith::core::DocumentState& document,
                bool include_opaque = false);
json to_payload(const polysmith::core::SessionState& session);
json to_payload(const polysmith::core::ViewportBoxPrimitive& primitive);
json to_payload(const polysmith::core::ViewportCylinderPrimitive& primitive);
json to_payload(const polysmith::core::ViewportPolygonExtrudePrimitive& primitive);
json to_payload(const polysmith::core::ViewportSolidFace& face);
json to_payload(const polysmith::core::ViewportSketchLinePrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchCirclePrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchArcPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchVertexPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchDimensionPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchConstraintPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchProfilePrimitive& primitive);
json to_payload(const polysmith::core::ViewportToolpathPrimitive& primitive);
json to_payload(const polysmith::core::ViewportState& viewport);

// ── CAM types (cam_types.h) ─────────────────────────────────────

json to_payload(const polysmith::core::Bounds3D& b);
json to_payload(const polysmith::core::FaceAttestation& att);
json to_payload(const polysmith::core::EdgeAttestation& att);
json to_payload(const polysmith::core::SketchProfileAttestation& att);
json to_payload(const polysmith::core::LaserCutParameters& laser);
json to_payload(const polysmith::core::LaserTestPatternParameters& pattern);
json to_payload(const polysmith::core::LaserMachineSettings& machine);
json to_payload(const polysmith::core::GeometryReference& ref);
json to_payload(const polysmith::core::StockDefinition& s);
json to_payload(const polysmith::core::MachineAxes& ma);
json to_payload(const polysmith::core::WcsOrigin& wcs);
json to_payload(const polysmith::core::CamSetup& setup);
json to_payload(const polysmith::core::ToolEntry& tool);
json to_payload(const polysmith::core::CamGeometryReferences& geo);
json to_payload(const polysmith::core::CamOperationParameters& p);
json to_payload(const polysmith::core::CamOperationDependencies& d);
json to_payload(const polysmith::core::ExternalStorage& es);
json to_payload(const polysmith::core::ToolpathCache& tc);
json to_payload(const polysmith::core::CamOperation& op);
json to_payload(const polysmith::core::PostProcessor& pp);
json to_payload(const polysmith::core::MachineDefinition& machine);
json to_payload(const polysmith::core::CamDocumentData& cam);

polysmith::core::ExtrudeFeatureParameters extrude_parameters_from_payload(
    const json& payload);

// Inverse of `to_payload(DocumentState)`. Used when loading a saved
// `.polysmith` document back into the core. Throws std::runtime_error on
// malformed payloads.
polysmith::core::FeatureEntry feature_entry_from_payload(const json& payload);
polysmith::core::DocumentState document_from_payload(const json& payload);

// Inverse of to_payload(CamDocumentData). Missing fields fall back to
// struct defaults so documents saved before a field existed still load.
polysmith::core::CamDocumentData cam_document_data_from_payload(
    const json& payload);

// ── CAM per-type inverses (used by the app command handlers) ──────

polysmith::core::CamSetup cam_setup_from_payload(const json& payload);
polysmith::core::StockDefinition stock_definition_from_payload(
    const json& payload);
polysmith::core::ToolEntry tool_entry_from_payload(const json& payload);
polysmith::core::CamOperation cam_operation_from_payload(const json& payload);
polysmith::core::CamGeometryReferences cam_geometry_references_from_payload(
    const json& payload);
polysmith::core::CamOperationParameters cam_operation_parameters_from_payload(
    const json& payload);
polysmith::core::LaserTestPatternParameters
laser_test_pattern_parameters_from_payload(const json& payload);
polysmith::core::LaserMachineSettings laser_machine_settings_from_payload(
    const json& payload);
polysmith::core::CamOperationDependencies cam_operation_dependencies_from_payload(
    const json& payload);
polysmith::core::PostProcessor post_processor_from_payload(const json& payload);
polysmith::core::MachineDefinition machine_definition_from_payload(
    const json& payload);

}  // namespace polysmith::protocol
