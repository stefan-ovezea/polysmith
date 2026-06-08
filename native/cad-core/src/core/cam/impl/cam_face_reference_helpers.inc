// ── Helpers ───────────────────────────────────────────────────────

double point_distance(const gp_Pnt& a, const gp_Pnt& b) {
  return std::sqrt((a.X() - b.X()) * (a.X() - b.X()) +
                   (a.Y() - b.Y()) * (a.Y() - b.Y()) +
                   (a.Z() - b.Z()) * (a.Z() - b.Z()));
}

double normal_angle(const gp_Dir& a, const gp_Dir& b) {
  const double dot = a.X() * b.X() + a.Y() * b.Y() + a.Z() * b.Z();
  // Clamp to [-1, 1] to avoid acos domain errors from float imprecision.
  const double clamped = std::max(-1.0, std::min(1.0, dot));
  return std::acos(clamped);
}

double compute_face_area(const TopoDS_Face& face) {
  try {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.Mass();
  } catch (const std::exception&) {
    return 0.0;
  }
}

bool is_point_on_face(const gp_Pnt& pnt, const TopoDS_Face& face,
                      double tolerance = 1e-3) {
  try {
    BRepClass_FaceClassifier classifier;
    // tolerance = maximum distance from face surface to consider "on".
    classifier.Perform(face, pnt, tolerance);
    return classifier.State() == TopAbs_ON ||
           classifier.State() == TopAbs_IN;
  } catch (const std::exception&) {
    return false;
  }
}

// Sample `count` points on the face surface, distributed across the
// UV parameter domain. Returns the sample points in world coordinates.
std::vector<gp_Pnt> sample_face_points(const TopoDS_Face& face, int count = 10) {
  std::vector<gp_Pnt> points;
  try {
    BRepAdaptor_Surface surface(face);
    const double uMin = surface.FirstUParameter();
    const double uMax = surface.LastUParameter();
    const double vMin = surface.FirstVParameter();
    const double vMax = surface.LastVParameter();

    // Distribute points in a grid pattern across the UV domain.
    // Use roughly sqrt(count) divisions in each direction.
    const int divisions = static_cast<int>(std::ceil(std::sqrt(
        static_cast<double>(count))));
    for (int i = 0; i < divisions; ++i) {
      for (int j = 0; j < divisions; ++j) {
        if (static_cast<int>(points.size()) >= count) break;
        const double u = uMin + (uMax - uMin) * (i + 0.5) /
                                     static_cast<double>(divisions);
        const double v = vMin + (vMax - vMin) * (j + 0.5) /
                                     static_cast<double>(divisions);
        gp_Pnt p;
        gp_Vec normal;
        surface.D0(u, v, p);
        points.push_back(p);
      }
      if (static_cast<int>(points.size()) >= count) break;
    }
  } catch (const std::exception&) {
    // Return whatever we sampled before the error.
  }
  return points;
}

// Compute the normal at the face's UV center.
std::optional<gp_Dir> face_center_normal(const TopoDS_Face& face) {
  try {
    BRepAdaptor_Surface surface(face);
    const double uMid =
        0.5 * (surface.FirstUParameter() + surface.LastUParameter());
    const double vMid =
        0.5 * (surface.FirstVParameter() + surface.LastVParameter());
    gp_Pnt center;
    gp_Vec d1u, d1v;
    surface.D1(uMid, vMid, center, d1u, d1v);
    gp_Vec normal = d1u.Crossed(d1v);
    if (normal.Magnitude() <= 1e-12) {
      return std::nullopt;
    }
    normal.Normalize();
    if (face.Orientation() == TopAbs_REVERSED) {
      normal.Reverse();
    }
    return gp_Dir(normal);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Score a candidate face against witness data. Returns 0.0 (no match)
// to 1.0 (perfect match).
double score_candidate(const CamFaceReference& reference,
                       const TopoDS_Face& face) {
  double score = 0.0;

  // ── Sample point containment (weight: 0.5) ──────────────────
  if (!reference.samplePoints.empty()) {
    int hits = 0;
    for (const auto& sp : reference.samplePoints) {
      gp_Pnt p(sp[0], sp[1], sp[2]);
      if (is_point_on_face(p, face)) {
        ++hits;
      }
    }
    const double hitRatio =
        static_cast<double>(hits) / reference.samplePoints.size();
    score += 0.5 * hitRatio;
  }

  // ── Area match (weight: 0.25) ───────────────────────────────
  const double area = compute_face_area(face);
  if (reference.capturedArea > 0.0 && area > 0.0) {
    const double ratio = std::min(area, reference.capturedArea) /
                         std::max(area, reference.capturedArea);
    score += 0.25 * ratio;
  }

  // ── Normal match (weight: 0.25) ─────────────────────────────
  const auto normal = face_center_normal(face);
  if (normal.has_value()) {
    const gp_Dir refNormal(reference.capturedNormal[0],
                           reference.capturedNormal[1],
                           reference.capturedNormal[2]);
    const double angle = normal_angle(*normal, refNormal);
    // Score drops linearly from 1.0 at 0° to 0.0 at kMaxNormalAngle.
    const double normalScore =
        1.0 - std::min(1.0, angle / kMaxNormalAngle);
    score += 0.25 * normalScore;
  }

  return score;
}
