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
//   - everything else -> "polygon" outline: straight edges become
//     corners, partial circular edges become arc descriptors
//     (polygon_segment_arcs), circular inner wires become
//     inner_circles; other curve types fall back to endpoint chords.
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
  // Walk the wire with curve identity: straight edges become corners,
  // partial circular edges become arc descriptors (fillet/chamfer
  // corners arrive as exact arcs instead of chords), anything else
  // falls back to the endpoint-chord behavior.
  walk_wire_loop(outer, outline.polygon_corners, outline.polygon_segment_arcs);
  inner_wire_geometry(face, outer, outline.inner_loops,
                      outline.inner_segment_arcs, outline.inner_circles);

  // Triangle-derived faces (converted meshes) share boundaries with
  // many collinear edges — merge collinear runs so projected sketches
  // stay small instead of emitting one line per original triangle
  // edge. The merge is arc-aware: corners adjacent to an arc never
  // collapse.
  merge_outline_loop(outline.polygon_corners, outline.polygon_segment_arcs);
  for (size_t i = 0; i < outline.inner_loops.size(); ++i) {
    merge_outline_loop(outline.inner_loops[i], outline.inner_segment_arcs[i]);
  }

  // Then decimate chord-sampled curved outlines (Douglas-Peucker at
  // 0.05 mm — below STL facet resolution and print tolerance), keeping
  // every arc junction corner so arc segments can never be orphaned.
  constexpr double kOutlineSimplifyTolerance = 0.05;
  const auto must_keep_for = [](const auto& segment_arcs) {
    std::vector<bool> mask(segment_arcs.size(), false);
    const size_t n = segment_arcs.size();
    for (size_t i = 0; i < n; ++i) {
      mask[i] = segment_arcs[(i + n - 1) % n].has_value() ||
                segment_arcs[i].has_value();
    }
    return mask;
  };
  // DP decimation drops corners; the parallel segment-arc lists must
  // follow the same kept indices or the count validation (and the arc
  // alignment) desyncs from the simplified loops.
  const auto subset_arcs = [](const auto& segment_arcs,
                              const std::vector<size_t>& kept) {
    std::vector<std::optional<FaceOutlineArc>> subset;
    subset.reserve(kept.size());
    for (const size_t index : kept) {
      subset.push_back(segment_arcs[index]);
    }
    return subset;
  };
  const auto outer_must_keep = must_keep_for(outline.polygon_segment_arcs);
  std::vector<size_t> outer_kept;
  outline.polygon_corners =
      simplify_outline_polyline(outline.polygon_corners,
                                kOutlineSimplifyTolerance, &outer_must_keep,
                                &outer_kept);
  outline.polygon_segment_arcs =
      subset_arcs(outline.polygon_segment_arcs, outer_kept);
  for (size_t i = 0; i < outline.inner_loops.size(); ++i) {
    const auto inner_must_keep = must_keep_for(outline.inner_segment_arcs[i]);
    std::vector<size_t> inner_kept;
    outline.inner_loops[i] =
        simplify_outline_polyline(outline.inner_loops[i],
                                  kOutlineSimplifyTolerance, &inner_must_keep,
                                  &inner_kept);
    outline.inner_segment_arcs[i] =
        subset_arcs(outline.inner_segment_arcs[i], inner_kept);
  }

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
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
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
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
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
