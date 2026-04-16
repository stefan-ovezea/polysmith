#include "protocol/serialization.h"

namespace polysmith::protocol {

json to_payload(const polysmith::core::FeatureEntry& feature) {
  return {
      {"feature_id", feature.id},
      {"kind", feature.kind},
      {"name", feature.name},
      {"status", feature.status},
      {"parameters_summary", feature.parameters_summary},
      {"box_parameters",
       feature.box_parameters.has_value()
           ? json{
                 {"width", feature.box_parameters->width},
                 {"height", feature.box_parameters->height},
                 {"depth", feature.box_parameters->depth},
             }
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::DocumentState& document) {
  json feature_history = json::array();
  for (const auto& feature : document.feature_history) {
    feature_history.push_back(to_payload(feature));
  }

  return {
      {"document_id", document.id},
      {"name", document.name},
      {"units", document.units},
      {"revision", document.revision},
      {"selected_feature_id",
       document.selected_feature_id.has_value()
           ? json(document.selected_feature_id.value())
           : json(nullptr)},
      {"feature_history", feature_history},
  };
}

json to_payload(const polysmith::core::SessionState& session) {
  json payload = {
      {"document_count", session.document_count},
      {"has_active_document", session.active_document_id.has_value()},
      {"can_undo", session.can_undo},
      {"can_redo", session.can_redo},
  };

  if (session.active_document_id.has_value()) {
    payload["active_document_id"] = session.active_document_id.value();
  } else {
    payload["active_document_id"] = nullptr;
  }

  return payload;
}

json to_payload(const polysmith::core::ViewportBoxPrimitive& primitive) {
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"width", primitive.width},
      {"height", primitive.height},
      {"depth", primitive.depth},
      {"x_offset", primitive.x_offset},
      {"is_selected", primitive.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportState& viewport) {
  json boxes = json::array();
  for (const auto& box : viewport.boxes) {
    boxes.push_back(to_payload(box));
  }

  return {
      {"has_active_document", viewport.has_active_document},
      {"boxes", boxes},
      {"scene_width", viewport.scene_width},
      {"scene_height", viewport.scene_height},
      {"scene_depth", viewport.scene_depth},
  };
}

}  // namespace polysmith::protocol
