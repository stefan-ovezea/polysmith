// ── Sketch text entity commands ────────────────────────────────
// Manage the parametric `texts` records. The glyph geometry is derived
// by refresh_sketch_texts inside refresh_sketch_derived_state, so each
// command just mutates the record and refreshes.

namespace {
// Upper bound on text length — long strings tessellate into tens of
// thousands of segments and make the profile face walk quadratic-blow.
constexpr size_t kMaxSketchTextChars = 500;
}  // namespace

void add_sketch_text(FeatureEntry& feature,
                     int text_index,
                     const SketchText& text) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch text");
  }
  if (text.text.size() > kMaxSketchTextChars) {
    throw std::runtime_error(
        "Sketch text exceeds the " +
        std::to_string(kMaxSketchTextChars) + " character limit");
  }
  if (text.height_mm <= 0.0) {
    throw std::runtime_error("Sketch text height must be positive");
  }
  if (text.h_align != "left" && text.h_align != "center" &&
      text.h_align != "right") {
    throw std::runtime_error("Unsupported sketch text h_align: " +
                             text.h_align);
  }
  if (text.v_align != "top" && text.v_align != "middle" &&
      text.v_align != "bottom") {
    throw std::runtime_error("Unsupported sketch text v_align: " +
                             text.v_align);
  }

  SketchText entry = text;
  entry.id = "text-" + std::to_string(text_index);
  feature.sketch_parameters->texts.push_back(std::move(entry));

  refresh_sketch_derived_state(feature);
}

void update_sketch_text(FeatureEntry& feature,
                        const std::string& text_id,
                        const SketchText& values) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch text");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto text_it = std::find_if(
      parameters.texts.begin(), parameters.texts.end(),
      [&](const SketchText& entry) { return entry.id == text_id; });
  if (text_it == parameters.texts.end()) {
    throw std::runtime_error("Sketch text not found: " + text_id);
  }
  if (values.text.size() > kMaxSketchTextChars) {
    throw std::runtime_error(
        "Sketch text exceeds the " +
        std::to_string(kMaxSketchTextChars) + " character limit");
  }
  if (values.height_mm <= 0.0) {
    throw std::runtime_error("Sketch text height must be positive");
  }

  // The caller (document manager / IPC handler) already merged the
  // partial update into `values`; the id is owned by the entity.
  SketchText merged = values;
  merged.id = text_id;
  *text_it = std::move(merged);

  refresh_sketch_derived_state(feature);
}

void delete_sketch_text(FeatureEntry& feature, const std::string& text_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch text");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto text_it = std::find_if(
      parameters.texts.begin(), parameters.texts.end(),
      [&](const SketchText& entry) { return entry.id == text_id; });
  if (text_it == parameters.texts.end()) {
    throw std::runtime_error("Sketch text not found: " + text_id);
  }
  parameters.texts.erase(text_it);

  // The strip pass in refresh_sketch_texts removes the generated glyph
  // lines; refresh also rebuilds profiles so downstream extrudes react.
  refresh_sketch_derived_state(feature);
}
