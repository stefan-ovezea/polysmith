// Test whether two solid shapes intersect with non-zero volume. Used by
// the auto-cut detector at extrude-creation time so a new_body extrude
// that overlaps an existing body is silently promoted to a cut.
bool shapes_intersect_with_volume(const TopoDS_Shape& a,
                                  const TopoDS_Shape& b) {
  if (a.IsNull() || b.IsNull()) {
    return false;
  }
  try {
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) {
      return false;
    }
    const TopoDS_Shape result = common.Shape();
    if (result.IsNull()) {
      return false;
    }
    // Any solid in the result -> the inputs share volume. We don't
    // dilute to TopAbs_FACE because two adjacent (touching but not
    // overlapping) solids share faces without sharing volume, and
    // promoting that to a cut would surprise the user.
    if (TopExp_Explorer(result, TopAbs_SOLID).More()) {
      return true;
    }
    // No solid — fall through to the bounding-box fallback below (the
    // fuzzy boolean can miss overlaps involving compounds).
  } catch (const std::exception&) {
  }
  // Fallback: non-degenerate bounding-box overlap means the shapes
  // genuinely share volume, even when the fuzzy boolean above fails on
  // compounds or exactly-touching geometry.
  Bnd_Box box_a;
  BRepBndLib::Add(a, box_a);
  Bnd_Box box_b;
  BRepBndLib::Add(b, box_b);
  const double overlap_x =
      std::min(box_a.CornerMax().X(), box_b.CornerMax().X()) -
      std::max(box_a.CornerMin().X(), box_b.CornerMin().X());
  const double overlap_y =
      std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y()) -
      std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y());
  const double overlap_z =
      std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z()) -
      std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z());
  if (overlap_x <= 0.0 || overlap_y <= 0.0 || overlap_z <= 0.0) {
    return false;
  }
  // Same noise tolerance as the touching check: real overlaps exceed
  // 1e-3 mm^3.
  return overlap_x * overlap_y * overlap_z > 1e-3;
}

bool shapes_touch_without_volume(const TopoDS_Shape& a,
                                 const TopoDS_Shape& b) {
  if (a.IsNull() || b.IsNull()) {
    return false;
  }
  try {
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) {
      return false;
    }
    const TopoDS_Shape result = common.Shape();
    if (result.IsNull()) {
      return false;
    }
    const bool has_solid = TopExp_Explorer(result, TopAbs_SOLID).More();
    const bool has_face = TopExp_Explorer(result, TopAbs_FACE).More();
    const bool has_edge = TopExp_Explorer(result, TopAbs_EDGE).More();
    const bool has_vertex = TopExp_Explorer(result, TopAbs_VERTEX).More();
    if (has_solid) {
      // Degenerate boolean result: shapes that touch face-to-face can
      // be "absorbed" by the fuzzy common, which then equals one of the
      // inputs.  Disambiguate with the bounding-box overlap volume:
      // a genuine volume overlap has a non-degenerate box intersection,
      // while face-touching shapes intersect in a zero-volume slab.
      Bnd_Box box_a;
      BRepBndLib::Add(a, box_a);
      Bnd_Box box_b;
      BRepBndLib::Add(b, box_b);
      const double overlap_x =
          std::min(box_a.CornerMax().X(), box_b.CornerMax().X()) -
          std::max(box_a.CornerMin().X(), box_b.CornerMin().X());
      const double overlap_y =
          std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y()) -
          std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y());
      const double overlap_z =
          std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z()) -
          std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z());
      const double overlap_volume =
          overlap_x > 0.0 && overlap_y > 0.0 && overlap_z > 0.0
              ? overlap_x * overlap_y * overlap_z
              : 0.0;
      // Numeric noise makes "touching" boxes overlap by ~1e-5 mm^3;
      // anything below 1e-3 mm^3 is treated as a coincident face.
      const bool genuine_overlap = overlap_volume > 1e-3;
      return !genuine_overlap;
    }
    if (has_face || has_edge || has_vertex) {
      return true;
    }
  } catch (const std::exception&) {
  }
  // Fallback: bounding-box analysis.  Touching shapes share a
  // zero-volume box intersection with a coincident boundary (gap ≈ 0);
  // genuinely overlapping shapes have a non-degenerate overlap volume,
  // and disjoint shapes have a positive gap.
  Bnd_Box box_a;
  BRepBndLib::Add(a, box_a);
  Bnd_Box box_b;
  BRepBndLib::Add(b, box_b);
  const double overlap_x =
      std::min(box_a.CornerMax().X(), box_b.CornerMax().X()) -
      std::max(box_a.CornerMin().X(), box_b.CornerMin().X());
  const double overlap_y =
      std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y()) -
      std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y());
  const double overlap_z =
      std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z()) -
      std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z());
  const double overlap_volume =
      overlap_x > 0.0 && overlap_y > 0.0 && overlap_z > 0.0
          ? overlap_x * overlap_y * overlap_z
          : 0.0;
  const double gap_x =
      std::max(box_a.CornerMin().X(), box_b.CornerMin().X()) -
      std::min(box_a.CornerMax().X(), box_b.CornerMax().X());
  const double gap_y =
      std::max(box_a.CornerMin().Y(), box_b.CornerMin().Y()) -
      std::min(box_a.CornerMax().Y(), box_b.CornerMax().Y());
  const double gap_z =
      std::max(box_a.CornerMin().Z(), box_b.CornerMin().Z()) -
      std::min(box_a.CornerMax().Z(), box_b.CornerMax().Z());
  const double gap = std::max(gap_x, std::max(gap_y, gap_z));
  constexpr double kTouchTolerance = 1e-4;
  // Same noise tolerance as the solid branch above.
  return overlap_volume <= 1e-3 && gap <= kTouchTolerance;
}

// Check whether a candidate extrude (built from `parameters`) would
// overlap any existing body in `document`. Returns the body id of the
// first such body, or nullopt when no intersection is found. Bodies
// derived from features after the candidate (in feature_history order)
// are excluded — we only want to detect overlap with bodies that exist
// "now" from the user's point of view.
std::optional<std::string> find_intersecting_body_for_extrude(
    const DocumentState& document,
    const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) {
      continue;
    }
    if (shapes_intersect_with_volume(body.shape, candidate)) {
      return body.id;
    }
  }
  return std::nullopt;
}

std::optional<std::string> find_touching_body_for_extrude(
    const DocumentState& document,
    const ExtrudeFeatureParameters& parameters) {
  TopoDS_Shape candidate;
  try {
    candidate = build_extrude_shape(parameters);
  } catch (const std::exception&) {
    return std::nullopt;
  }
  if (candidate.IsNull()) {
    return std::nullopt;
  }

  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) {
      continue;
    }
    if (shapes_touch_without_volume(body.shape, candidate)) {
      return body.id;
    }
  }
  return std::nullopt;
}
