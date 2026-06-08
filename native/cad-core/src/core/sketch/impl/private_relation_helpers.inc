void remove_line_relations_for_line(SketchFeatureParameters& parameters,
                                    const std::string& kind,
                                    const std::string& line_id) {
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& relation) {
                       return relation.kind == kind &&
                              (relation.first_line_id == line_id ||
                               relation.second_line_id == line_id);
                     }),
      parameters.line_relations.end());
}

bool line_has_active_fillet(const SketchFeatureParameters& parameters,
                            const std::string& line_id) {
  return std::any_of(
      parameters.fillets.begin(),
      parameters.fillets.end(),
      [&](const SketchFillet& fillet) {
        return fillet.line_a_id == line_id || fillet.line_b_id == line_id;
      });
}
