// Returns true when `suffix` parses as a non-negative integer — that's
// the body-derived face id format ("<body_id>:face:<index>"), distinct
// from the legacy named suffixes ("top", "base", "side-N", etc.).
bool suffix_is_numeric_index(const std::string& suffix, int& index_out) {
  if (suffix.empty()) {
    return false;
  }
  try {
    size_t consumed = 0;
    const int value = std::stoi(suffix, &consumed);
    if (consumed != suffix.size() || value < 0) {
      return false;
    }
    index_out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

// Walk `face`'s outer wire and produce a world-space outline. Detects:
//   - a wire with a single circle edge -> "circle" outline
//   - everything else -> "polygon" outline (line segments only;
//     curved edges are sampled by their two endpoints, which is a
//     reasonable approximation when the user projects a face whose
//     outer wire happens to include arcs).
// Returns nullopt only when the wire couldn't be walked at all.
std::optional<FaceOutline> outline_from_face(const TopoDS_Face& face) {
  TopoDS_Wire outer;
  try {
    outer = BRepTools::OuterWire(face);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (outer.IsNull()) {
    return std::nullopt;
  }

  // Collect edges in order around the wire so the projected polygon's
  // corners follow the face's actual outline.
  const std::vector<TopoDS_Edge> ordered_edges = ordered_wire_edges(outer);
  if (ordered_edges.empty()) {
    return std::nullopt;
  }

  // Circular and annular planar faces should project as sketch circles,
  // not as sampled polygon loops. If every loop is circular, keep that
  // exact semantic shape and let the projector emit one circle per loop.
  if (const auto outer_circle = circle_from_wire(outer);
      outer_circle.has_value()) {
    if (const auto inner_circles = inner_wire_circles(face, outer);
        inner_circles.has_value()) {
      FaceOutline outline{};
      outline.kind = "circle";
      outline.circle_center = outer_circle->center;
      outline.circle_axis = outer_circle->axis;
      outline.circle_radius = outer_circle->radius;
      outline.inner_circles = inner_circles.value();
      return outline;
    }
  }

  FaceOutline outline{};
  outline.kind = "polygon";
  outline.polygon_corners = sample_wire_loop(outer);
  outline.inner_loops = inner_wire_loops(face, outer);

  if (outline.polygon_corners.size() < 3) {
    return std::nullopt;
  }

  return outline;
}

// Resolve a numeric face id ("<body_id>:face:<index>") by recompiling
// bodies, finding the body whose root id matches `body_id`, and walking
// `TopExp::MapShapes(TopAbs_FACE)` to retrieve the requested face. The
// recompile mirrors what the viewport already does — it's the source of
// truth for body topology, so it's also the source of truth for face
// resolution.
std::optional<FaceOutline> outline_for_body_face(
    const DocumentState& document,
    const std::string& body_id,
    int face_index) {
  if (face_index < 0) {
    return std::nullopt;
  }
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.id != body_id || body.shape.IsNull()) {
      continue;
    }
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
    TopExp::MapShapes(body.shape, TopAbs_FACE, face_map);
    const int one_based = face_index + 1;
    if (one_based < 1 || one_based > face_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(face_map(one_based));
    if (face.IsNull()) {
      return std::nullopt;
    }
    return outline_from_face(face);
  }
  return std::nullopt;
}

std::optional<TopoDS_Face> resolve_body_face(const DocumentState& document,
                                             const std::string& body_id,
                                             int face_index) {
  if (face_index < 0) {
    return std::nullopt;
  }
  const CompiledBodies compiled = compile_bodies(document);
  for (const auto& body : compiled.bodies) {
    if (body.id != body_id || body.shape.IsNull()) {
      continue;
    }
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
    TopExp::MapShapes(body.shape, TopAbs_FACE, face_map);
    const int one_based = face_index + 1;
    if (one_based < 1 || one_based > face_map.Extent()) {
      return std::nullopt;
    }
    const TopoDS_Face face = TopoDS::Face(face_map(one_based));
    if (face.IsNull()) {
      return std::nullopt;
    }
    return face;
  }
  return std::nullopt;
}
