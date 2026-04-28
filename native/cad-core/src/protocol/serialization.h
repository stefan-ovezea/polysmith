#pragma once

#include <nlohmann/json.hpp>

#include "core/document.h"
#include "core/viewport.h"

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
json to_payload(const polysmith::core::ViewportSketchPointPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchDimensionPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchConstraintPrimitive& primitive);
json to_payload(const polysmith::core::ViewportSketchProfilePrimitive& primitive);
json to_payload(const polysmith::core::ViewportState& viewport);

}  // namespace polysmith::protocol
