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
    TopExp_Explorer explorer(result, TopAbs_SOLID);
    return explorer.More();
  } catch (const std::exception&) {
    return false;
  }
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
    if (TopExp_Explorer(result, TopAbs_SOLID).More()) {
      return false;
    }
    if (TopExp_Explorer(result, TopAbs_FACE).More() ||
        TopExp_Explorer(result, TopAbs_EDGE).More() ||
        TopExp_Explorer(result, TopAbs_VERTEX).More()) {
      return true;
    }
  } catch (const std::exception&) {
  }
  try {
    BRepExtrema_DistShapeShape distance(a, b);
    distance.Perform();
    return distance.IsDone() && distance.Value() <= 1.0e-6;
  } catch (const std::exception&) {
    return false;
  }
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

  const CompiledBodies compiled = compile_bodies(document);
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

  const CompiledBodies compiled = compile_bodies(document);
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
