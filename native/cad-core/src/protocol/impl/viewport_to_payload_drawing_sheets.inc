// ── Drawing sheet payloads (viewport_drawing_primitives.h) ────────

namespace {

json drawing_vec2(const std::array<double, 2>& p) {
  return json::array({p[0], p[1]});
}

}  // namespace

json to_payload(const polysmith::core::ViewportDrawingCurve& curve) {
  json j{
      {"kind", curve.kind},
      {"line_class", curve.line_class},
      {"curve_class", curve.curve_class},
      {"purpose", curve.purpose},
      {"width_mm", curve.width_mm},
      {"p0", drawing_vec2(curve.p0)},
      {"p1", drawing_vec2(curve.p1)},
      {"start_angle", curve.start_angle},
      {"end_angle", curve.end_angle},
  };
  if (curve.center.has_value()) j["center"] = drawing_vec2(curve.center.value());
  if (curve.radius.has_value()) j["radius"] = curve.radius.value();
  if (curve.major_dir.has_value()) j["major_dir"] = drawing_vec2(curve.major_dir.value());
  if (curve.major_radius.has_value()) j["major_radius"] = curve.major_radius.value();
  if (curve.minor_radius.has_value()) j["minor_radius"] = curve.minor_radius.value();
  if (!curve.points.empty()) {
    json points = json::array();
    for (const auto& p : curve.points) points.push_back(drawing_vec2(p));
    j["points"] = points;
  }
  return j;
}

json to_payload(const polysmith::core::ViewportDrawingView& view) {
  return {
      {"view_id", view.view_id},
      {"label", view.label},
      {"scale", view.scale},
      {"origin", drawing_vec2(view.origin)},
      {"min", drawing_vec2(view.min)},
      {"max", drawing_vec2(view.max)},
      {"stale", view.stale},
      {"warning", view.warning},
  };
}

json to_payload(const polysmith::core::ViewportDrawingText& text) {
  json j{
      {"text", text.text},
      {"position", drawing_vec2(text.position)},
      {"height_mm", text.height_mm},
      {"angle_deg", text.angle_deg},
      {"h_align", text.h_align},
      {"purpose", text.purpose},
      {"stale", text.stale},
  };
  if (text.annotation_id.has_value()) {
    j["annotation_id"] = text.annotation_id.value();
  }
  return j;
}

json to_payload(const polysmith::core::ViewportDrawingSheet& sheet) {
  json curves = json::array();
  for (const auto& curve : sheet.curves) curves.push_back(to_payload(curve));
  json views = json::array();
  for (const auto& view : sheet.views) views.push_back(to_payload(view));
  json texts = json::array();
  for (const auto& text : sheet.texts) texts.push_back(to_payload(text));
  return {
      {"sheet_id", sheet.sheet_id},
      {"drawing_id", sheet.drawing_id},
      {"name", sheet.name},
      {"width_mm", sheet.width_mm},
      {"height_mm", sheet.height_mm},
      {"curves", curves},
      {"views", views},
      {"texts", texts},
  };
}
