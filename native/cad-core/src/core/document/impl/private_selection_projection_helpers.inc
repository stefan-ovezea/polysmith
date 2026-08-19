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

std::optional<TopoDS_Face> resolve_face_for_appearance(
    const DocumentState& document,
    const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return std::nullopt;
  }
  const std::string owner_id = face_id.substr(0, separator);
  int index = -1;
  try {
    index = std::stoi(face_id.substr(separator + 6));
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (index < 0) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  const auto body_it =
      std::find_if(compiled.bodies.begin(),
                   compiled.bodies.end(),
                   [&](const CompiledBody& body) { return body.id == owner_id; });
  if (body_it == compiled.bodies.end() || body_it->shape.IsNull()) {
    return std::nullopt;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(body_it->shape, TopAbs_FACE, face_map);
  const int one_based = index + 1;
  if (one_based < 1 || one_based > face_map.Extent()) {
    return std::nullopt;
  }
  const TopoDS_Face face = TopoDS::Face(face_map(one_based));
  if (face.IsNull()) {
    return std::nullopt;
  }
  return face;
}
