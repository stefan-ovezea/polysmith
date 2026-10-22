bool id_in_set(const std::unordered_set<std::string>& ids,
               const std::string& id) {
  return ids.find(id) != ids.end();
}

void add_id(std::unordered_set<std::string>& ids, const std::string& id) {
  if (!id.empty()) {
    ids.insert(id);
  }
}

void add_ids(std::unordered_set<std::string>& ids,
             const std::vector<std::string>& source) {
  for (const auto& id : source) {
    add_id(ids, id);
  }
}

void remove_ids_from_vector(std::vector<std::string>& ids,
                            const std::unordered_set<std::string>& deleted_ids) {
  ids.erase(std::remove_if(ids.begin(),
                           ids.end(),
                           [&](const std::string& id) {
                             return id_in_set(deleted_ids, id);
                           }),
            ids.end());
}

bool projection_has_generated_entities(const SketchProjection& projection) {
  return !projection.generated_line_ids.empty() ||
         !projection.generated_circle_ids.empty() ||
         !projection.generated_arc_ids.empty() ||
         !projection.generated_vertex_id.empty();
}

bool projection_generates_entity(const SketchProjection& projection,
                                 const std::string& entity_id) {
  return std::find(projection.generated_line_ids.begin(),
                   projection.generated_line_ids.end(),
                   entity_id) != projection.generated_line_ids.end() ||
         std::find(projection.generated_circle_ids.begin(),
                   projection.generated_circle_ids.end(),
                   entity_id) != projection.generated_circle_ids.end() ||
         std::find(projection.generated_arc_ids.begin(),
                   projection.generated_arc_ids.end(),
                   entity_id) != projection.generated_arc_ids.end();
}

bool compiled_body_exists(const DocumentState& document,
                          const std::string& body_id) {
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  return std::any_of(compiled.bodies.begin(),
                     compiled.bodies.end(),
                     [&](const CompiledBody& body) {
                       return body.id == body_id;
                     });
}
