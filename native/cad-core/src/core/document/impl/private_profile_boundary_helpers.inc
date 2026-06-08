std::vector<SketchProfilePoint> sample_circle_profile_points(
    const SketchProfileRegion& profile) {
  std::vector<SketchProfilePoint> points;
  constexpr int kCircleSegments = 64;
  points.reserve(kCircleSegments);
  for (int index = 0; index < kCircleSegments; ++index) {
    const double angle =
        (static_cast<double>(index) / static_cast<double>(kCircleSegments)) *
        2.0 * 3.14159265358979323846;
    points.push_back(SketchProfilePoint{
        .x = profile.center_x + profile.radius * std::cos(angle),
        .y = profile.center_y + profile.radius * std::sin(angle),
    });
  }
  return points;
}

std::vector<SketchProfilePoint> profile_boundary_points(
    const SketchProfileRegion& profile) {
  return profile.kind == "circle" ? sample_circle_profile_points(profile)
                                  : profile.points;
}

double squared_distance(const SketchProfilePoint& a,
                        const SketchProfilePoint& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return dx * dx + dy * dy;
}

double cross(const SketchProfilePoint& a,
             const SketchProfilePoint& b,
             const SketchProfilePoint& c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_on_segment(const SketchProfilePoint& point,
                      const SketchProfilePoint& a,
                      const SketchProfilePoint& b,
                      double tolerance) {
  if (std::abs(cross(a, b, point)) > tolerance) {
    return false;
  }
  return point.x >= std::min(a.x, b.x) - tolerance &&
         point.x <= std::max(a.x, b.x) + tolerance &&
         point.y >= std::min(a.y, b.y) - tolerance &&
         point.y <= std::max(a.y, b.y) + tolerance;
}

bool segments_touch(const SketchProfilePoint& a1,
                    const SketchProfilePoint& a2,
                    const SketchProfilePoint& b1,
                    const SketchProfilePoint& b2,
                    double tolerance) {
  if (point_on_segment(a1, b1, b2, tolerance) ||
      point_on_segment(a2, b1, b2, tolerance) ||
      point_on_segment(b1, a1, a2, tolerance) ||
      point_on_segment(b2, a1, a2, tolerance)) {
    return true;
  }

  const double c1 = cross(a1, a2, b1);
  const double c2 = cross(a1, a2, b2);
  const double c3 = cross(b1, b2, a1);
  const double c4 = cross(b1, b2, a2);
  return ((c1 > tolerance && c2 < -tolerance) ||
          (c1 < -tolerance && c2 > tolerance)) &&
         ((c3 > tolerance && c4 < -tolerance) ||
          (c3 < -tolerance && c4 > tolerance));
}

bool profile_regions_touch(const SketchProfileRegion& a,
                           const SketchProfileRegion& b) {
  constexpr double kTolerance = 1.0e-6;
  constexpr double kToleranceSquared = kTolerance * kTolerance;
  const std::vector<SketchProfilePoint> a_points = profile_boundary_points(a);
  const std::vector<SketchProfilePoint> b_points = profile_boundary_points(b);
  if (a_points.empty() || b_points.empty()) {
    return false;
  }

  for (const auto& a_point : a_points) {
    for (const auto& b_point : b_points) {
      if (squared_distance(a_point, b_point) <= kToleranceSquared) {
        return true;
      }
    }
  }

  for (size_t a_index = 0; a_index < a_points.size(); ++a_index) {
    const auto& a1 = a_points[a_index];
    const auto& a2 = a_points[(a_index + 1) % a_points.size()];
    for (size_t b_index = 0; b_index < b_points.size(); ++b_index) {
      const auto& b1 = b_points[b_index];
      const auto& b2 = b_points[(b_index + 1) % b_points.size()];
      if (segments_touch(a1, a2, b1, b2, kTolerance)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<std::vector<SketchProfileRegion>> group_touching_profiles(
    const std::vector<SketchProfileRegion>& profiles) {
  std::vector<std::vector<SketchProfileRegion>> groups;
  std::vector<bool> visited(profiles.size(), false);

  for (size_t start = 0; start < profiles.size(); ++start) {
    if (visited[start]) {
      continue;
    }

    std::vector<size_t> stack{start};
    visited[start] = true;
    std::vector<SketchProfileRegion> group;

    while (!stack.empty()) {
      const size_t current = stack.back();
      stack.pop_back();
      group.push_back(profiles[current]);

      for (size_t candidate = 0; candidate < profiles.size(); ++candidate) {
        if (visited[candidate]) {
          continue;
        }
        if (!profile_regions_touch(profiles[current], profiles[candidate])) {
          continue;
        }
        visited[candidate] = true;
        stack.push_back(candidate);
      }
    }

    groups.push_back(std::move(group));
  }

  return groups;
}

bool profile_id_mentions_entity(const std::string& profile_id,
                                const std::string& entity_id) {
  const std::string needle = "-" + entity_id;
  size_t position = profile_id.find(needle);
  while (position != std::string::npos) {
    const size_t after = position + needle.size();
    if (after == profile_id.size() || profile_id[after] == '-') {
      return true;
    }
    position = profile_id.find(needle, position + 1);
  }
  return false;
}

const SketchProfileRegion* find_profile_by_id(
    const std::vector<SketchProfileRegion>& profiles,
    const std::string& profile_id) {
  const auto profile_it = std::find_if(
      profiles.begin(),
      profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  return profile_it == profiles.end() ? nullptr : &(*profile_it);
}

const SketchProfileRegion* find_equivalent_profile(
    const std::vector<SketchProfileRegion>& profiles,
    const std::string& source_profile_id) {
  if (const SketchProfileRegion* exact =
          find_profile_by_id(profiles, source_profile_id)) {
    return exact;
  }

  std::vector<const SketchProfileRegion*> candidates;
  for (const auto& profile : profiles) {
    if (profile.kind == "circle" && profile.source_circle_id.has_value() &&
        source_profile_id.find("profile-circle-" +
                               profile.source_circle_id.value()) == 0) {
      candidates.push_back(&profile);
      continue;
    }

    if (profile.kind == "polygon" && !profile.line_ids.empty()) {
      const bool same_entities = std::all_of(
          profile.line_ids.begin(),
          profile.line_ids.end(),
          [&](const std::string& entity_id) {
            return profile_id_mentions_entity(source_profile_id, entity_id);
          });
      if (same_entities) {
        candidates.push_back(&profile);
      }
    }
  }

  return candidates.size() == 1 ? candidates.front() : nullptr;
}
