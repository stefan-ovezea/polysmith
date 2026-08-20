// Parametric sketch text entities. The glyph geometry itself lives in
// the ordinary `lines` vector (generated_by = "text:<id>"); these
// commands only manage the parameter records in `parameters.texts`.
void add_sketch_text(FeatureEntry& feature,
                     int text_index,
                     const SketchText& text);
// Replaces the stored parameters of an existing text entity (the caller
// has already merged the partial update into `values`). Re-expands the
// glyph geometry through refresh_sketch_derived_state.
void update_sketch_text(FeatureEntry& feature,
                        const std::string& text_id,
                        const SketchText& values);
void delete_sketch_text(FeatureEntry& feature, const std::string& text_id);

// Strips previously generated entities and re-expands every text in
// `parameters.texts` into ordinary SketchLine segments. Called at the
// top of refresh_sketch_derived_state so every downstream pass (solver,
// vertex rebuild, profile walk) sees current text geometry.
void refresh_sketch_texts(SketchFeatureParameters& parameters);

// Throws when the line exists but is generated geometry (not editable).
void require_user_line(const SketchFeatureParameters& parameters,
                       const std::string& line_id);
// Throws when the entity id belongs to a generated line/circle/arc.
void ensure_user_editable_entity(const SketchFeatureParameters& parameters,
                                 const std::string& entity_id);
// Helper used by the guard functions above.
bool is_generated_entity(const SketchFeatureParameters& parameters,
                         const std::string& entity_id);
