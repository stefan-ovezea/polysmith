// ================================================================
// Constraint transfer on trim (spec D1/D4)
//
// Runs inside the trim commit: `capture_...` BEFORE the shared
// cleanup (trim_remove_entity_attachments erases everything the plan
// re-applies), `apply_...` after the entity mutation and the orphan
// sweeps, before the document layer's solver refresh.
//
// D1 whitelist:
//   - coincident: re-applied when every referenced point survives AND
//     still coincides (positions re-checked — a severed endpoint pair
//     keeps its ids alive, so id-liveness alone is not enough)
//   - horizontal / vertical: surviving line pieces keep the badge
//   - parallel / perpendicular / equal_length: re-related to the same
//     partner on every surviving piece
//   - point-on-object: a new cut endpoint gets a point_line_anchor
//     onto its cutting LINE (the solver's PointOnLine). No anchor kind
//     exists for curve cutters — those cuts stay free (documented).
//   - dimensions: re-derived as DRIVEN with measured values
//   - fillets/chamfers: always removed by the cleanup, never re-applied
//     (owned geometry cannot outlive its record)
//   - fixed constraints: never transfer (they pin specific ids)
//
// D4: concentric relations re-target onto the surviving arc when a
// circle is trimmed to an arc.
// ================================================================

// This TU (sketch_feature.cpp) does not define _USE_MATH_DEFINES —
// M_PI is unavailable on MSVC (same convention as extend_entity_commands.inc).
constexpr double kTransferPi = 3.14159265358979323846;

struct TrimConstraintTransfer {
  std::optional<std::string> line_hv;
  bool line_hv_driven = false;
  // (kind, partner_line_id) — the partner is the OTHER side of the
  // relation; every surviving piece is re-related to it.
  std::vector<std::pair<std::string, std::string>> line_relations;
  // The other entity of each concentric constraint on the trimmed
  // circle/arc.
  std::vector<std::string> concentric_others;
  struct DimSpec {
    std::string kind;
    std::string secondary_entity_id;
    std::string display_as;
  };
  std::vector<DimSpec> dim_specs;
  // Coincident constraints touching the trimmed entity / its points,
  // re-applied only when all targets survive AND still coincide.
  std::vector<SketchConstraint> coincident_snapshot;
};

TrimConstraintTransfer capture_trim_constraint_transfer(
    const SketchFeatureParameters& params, const std::string& entity_id,
    const std::vector<std::string>& point_ids) {
  TrimConstraintTransfer plan;

  auto touches = [&](const std::string& id) {
    if (id == entity_id) return true;
    for (const auto& pid : point_ids) {
      if (pid == id) return true;
    }
    return false;
  };

  // H/V badge on the source line.
  const auto line_it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const SketchLine& l) { return l.id == entity_id; });
  if (line_it != params.lines.end()) {
    plan.line_hv = line_it->constraint;
    plan.line_hv_driven = line_it->constraint_driven;
  }

  // Whitelisted line relations.
  for (const auto& rel : params.line_relations) {
    if (rel.first_line_id != entity_id && rel.second_line_id != entity_id) {
      continue;
    }
    if (rel.kind != "parallel" && rel.kind != "perpendicular" &&
        rel.kind != "equal_length") {
      continue;
    }
    plan.line_relations.push_back(
        {rel.kind, rel.first_line_id == entity_id ? rel.second_line_id
                                                  : rel.first_line_id});
  }

  // Concentric pairs (D4).
  for (const auto& c : params.constraints) {
    if (c.kind != "concentric" || c.target_ids.size() != 2) continue;
    if (c.target_ids[0] == entity_id) {
      plan.concentric_others.push_back(c.target_ids[1]);
    } else if (c.target_ids[1] == entity_id) {
      plan.concentric_others.push_back(c.target_ids[0]);
    }
  }

  // Dimensions to re-derive as driven.
  for (const auto& d : params.dimensions) {
    if (d.entity_id != entity_id) continue;
    plan.dim_specs.push_back({d.kind, d.secondary_entity_id, d.display_as});
  }

  // Coincident snapshot.
  for (const auto& c : params.constraints) {
    if (c.kind != "coincident") continue;
    bool any_touch = false;
    for (const auto& tid : c.target_ids) {
      if (touches(tid)) {
        any_touch = true;
        break;
      }
    }
    if (any_touch) plan.coincident_snapshot.push_back(c);
  }

  return plan;
}

// A surviving piece's cut endpoint — the point-on-object candidate.
struct TrimTransferCut {
  double x = 0.0;
  double y = 0.0;
  std::string point_id;
  std::string cutter_entity_id;  // empty = no anchor
};

// The cutter entity that produced a trimmed segment boundary. Segment
// parameters ARE intersection parameters (degenerate pieces are
// dropped by the splitters), so an exact match is the norm; the
// tolerance only absorbs float dust.
const std::string* cutter_for_param(
    const std::vector<TrimIntersection>& intersections, double param) {
  for (const auto& is : intersections) {
    if (std::abs(is.param_on_target - param) < 1e-9) {
      return &is.other_entity_id;
    }
  }
  return nullptr;
}

// Arc sweep angles from stored coords (local copy — the trim engine's
// arc_angles is TU-local to trim_engine.cpp, not visible here).
std::pair<double, double> transfer_arc_angles(const SketchArc& arc) {
  return {std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x),
          std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x)};
}

// Ellipse point at a parametric (eccentric-anomaly) angle — the same
// convention the ellipse splitter and the UI sampler use. The ellipse
// stores no endpoint coordinates, so the cut positions are derived
// from the sweep angles the trim command just wrote.
std::pair<double, double> transfer_ellipse_point(const SketchEllipse& e,
                                                 double t) {
  const double cu = std::cos(e.rotation);
  const double su = std::sin(e.rotation);
  const double px = e.a * std::cos(t);
  const double py = e.b * std::sin(t);
  return {e.center_x + px * cu - py * su, e.center_y + px * su + py * cu};
}

double transfer_arc_sweep(const SketchArc& arc) {
  auto [s, e] = transfer_arc_angles(arc);
  if (arc.ccw) {
    if (e <= s) e += 2.0 * kTransferPi;
    return e - s;
  }
  if (e >= s) e -= 2.0 * kTransferPi;
  return s - e;
}

// Measured value for a dimension kind on a surviving entity; nullopt
// when the kind cannot be re-derived (then no driven dim is created).
std::optional<double> transfer_dimension_value(
    const SketchFeatureParameters& params, const std::string& kind,
    const std::string& entity_id, const std::string& secondary_id) {
  if (kind == "line_length") {
    const auto it = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const SketchLine& l) { return l.id == entity_id; });
    if (it == params.lines.end()) return std::nullopt;
    return std::hypot(it->end_x - it->start_x, it->end_y - it->start_y);
  }
  if (kind == "line_angle") {
    const auto it = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const SketchLine& l) { return l.id == entity_id; });
    if (it == params.lines.end()) return std::nullopt;
    double a = std::atan2(it->end_y - it->start_y, it->end_x - it->start_x);
    if (a < 0.0) a += 2.0 * kTransferPi;
    return a;
  }
  if (kind == "circle_radius" || kind == "arc_radius") {
    for (const auto& c : params.circles) {
      if (c.id == entity_id) return c.radius;
    }
    for (const auto& a : params.arcs) {
      if (a.id == entity_id) return a.radius;
    }
    return std::nullopt;
  }
  if (kind == "arc_angle") {
    const auto it = std::find_if(
        params.arcs.begin(), params.arcs.end(),
        [&](const SketchArc& a) { return a.id == entity_id; });
    if (it == params.arcs.end()) return std::nullopt;
    return transfer_arc_sweep(*it);
  }
  if (kind == "arc_length") {
    const auto it = std::find_if(
        params.arcs.begin(), params.arcs.end(),
        [&](const SketchArc& a) { return a.id == entity_id; });
    if (it == params.arcs.end()) return std::nullopt;
    return transfer_arc_sweep(*it) * it->radius;
  }
  if (kind == "angle") {
    // Angle between the primary and secondary lines (both must survive).
    const auto find_line = [&](const std::string& id) -> const SketchLine* {
      for (const auto& l : params.lines) {
        if (l.id == id) return &l;
      }
      return nullptr;
    };
    const SketchLine* a = find_line(entity_id);
    const SketchLine* b = find_line(secondary_id);
    if (a == nullptr || b == nullptr) return std::nullopt;
    const double ax = a->end_x - a->start_x, ay = a->end_y - a->start_y;
    const double bx = b->end_x - b->start_x, by = b->end_y - b->start_y;
    const double cross = ax * by - ay * bx;
    const double dot = ax * bx + ay * by;
    if (std::abs(cross) < kTrimCoincidentTolerance &&
        std::abs(dot) < kTrimCoincidentTolerance) {
      return std::nullopt;  // degenerate line
    }
    return std::atan2(std::abs(cross), dot);
  }
  if (kind == "circle_center_distance") {
    const auto find_center = [&](const std::string& id)
        -> std::optional<std::pair<double, double>> {
      for (const auto& c : params.circles) {
        if (c.id == id) return std::pair{c.center_x, c.center_y};
      }
      for (const auto& a : params.arcs) {
        if (a.id == id) return std::pair{a.center_x, a.center_y};
      }
      return std::nullopt;
    };
    const auto a = find_center(entity_id);
    const auto b = find_center(secondary_id);
    if (!a.has_value() || !b.has_value()) return std::nullopt;
    return std::hypot(a->first - b->first, a->second - b->second);
  }
  if (kind == "circle_line_distance") {
    const auto find_center = [&](const std::string& id)
        -> std::optional<std::pair<double, double>> {
      for (const auto& c : params.circles) {
        if (c.id == id) return std::pair{c.center_x, c.center_y};
      }
      for (const auto& a : params.arcs) {
        if (a.id == id) return std::pair{a.center_x, a.center_y};
      }
      return std::nullopt;
    };
    const auto find_line = [&](const std::string& id) -> const SketchLine* {
      for (const auto& l : params.lines) {
        if (l.id == id) return &l;
      }
      return nullptr;
    };
    const auto center = find_center(entity_id);
    const SketchLine* line = find_line(secondary_id);
    if (!center.has_value() || line == nullptr) return std::nullopt;
    const double dx = line->end_x - line->start_x;
    const double dy = line->end_y - line->start_y;
    const double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) return std::nullopt;
    return std::abs((line->start_y - center->second) * dx -
                    (line->start_x - center->first) * dy) /
           std::sqrt(len2);
  }
  if (kind == "line_line_distance") {
    const auto find_line = [&](const std::string& id) -> const SketchLine* {
      for (const auto& l : params.lines) {
        if (l.id == id) return &l;
      }
      return nullptr;
    };
    const SketchLine* a = find_line(entity_id);
    const SketchLine* b = find_line(secondary_id);
    if (a == nullptr || b == nullptr) return std::nullopt;
    // Only meaningful for parallel lines.
    const double ax = a->end_x - a->start_x, ay = a->end_y - a->start_y;
    const double bx = b->end_x - b->start_x, by = b->end_y - b->start_y;
    if (std::abs(ax * by - ay * bx) > kTrimCoincidentTolerance) {
      return std::nullopt;
    }
    const double len2 = bx * bx + by * by;
    if (len2 < 1e-12) return std::nullopt;
    return std::abs((b->start_y - a->start_y) * bx -
                    (b->start_x - a->start_x) * by) /
           std::sqrt(len2);
  }
  // polygon_radius, point_distance and any unknown kind: skip — the
  // source semantics do not survive the trim.
  return std::nullopt;
}

// Coincident constraint records list the LINES sharing the merged
// point in `target_ids`; the merged POINT id is encoded in the
// constraint id ("constraint-coincident-<point_id>"). A record still
// holds when the merged point is live AND every listed line still
// references it as an endpoint (a trim rebind can hand a line a new
// endpoint id while the old shared id survives on the other line).
std::string coincident_point_id(const SketchConstraint& c) {
  const std::string prefix = "constraint-coincident-";
  if (c.kind == "coincident" && c.constraint_id.rfind(prefix, 0) == 0) {
    return c.constraint_id.substr(prefix.size());
  }
  return "";
}

bool line_references_point(const SketchFeatureParameters& params,
                           const std::string& line_id,
                           const std::string& point_id) {
  const auto it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const SketchLine& l) { return l.id == line_id; });
  if (it == params.lines.end()) return false;
  return it->start_vertex_id == point_id || it->end_vertex_id == point_id;
}

// Legacy records without the point suffix survive when every target
// id is still a live point id.
bool coincident_record_alive(
    const SketchFeatureParameters& params, const SketchConstraint& c,
    const std::unordered_map<std::string, std::pair<double, double>>&
        positions) {
  if (c.kind != "coincident") return false;
  const std::string pid = coincident_point_id(c);
  if (!pid.empty()) {
    if (!positions.count(pid)) return false;
    for (const auto& line_id : c.target_ids) {
      if (!line_references_point(params, line_id, pid)) return false;
    }
    return true;
  }
  for (const auto& tid : c.target_ids) {
    if (!positions.count(tid)) return false;
  }
  return true;
}

// Orphan sweep: erase coincident records whose merged point died or
// whose listed lines no longer reference it. Every trim path runs
// this after the cleanup (the old sweep compared the record's LINE
// targets against the live POINT set — line ids never match, so it
// silently erased every coincident constraint).
void sweep_orphan_coincident_constraints(SketchFeatureParameters& params) {
  const auto positions = live_sketch_point_positions(params);
  params.constraints.erase(
      std::remove_if(params.constraints.begin(), params.constraints.end(),
                     [&](const SketchConstraint& c) {
                       return c.kind == "coincident" &&
                              !coincident_record_alive(params, c, positions);
                     }),
      params.constraints.end());
}

void apply_trim_constraint_transfer(
    SketchFeatureParameters& params, const TrimConstraintTransfer& plan,
    const std::vector<std::string>& survivors,
    const std::vector<TrimTransferCut>& cuts) {
  // 1. H/V badge onto surviving line pieces.
  if (plan.line_hv.has_value()) {
    for (const auto& id : survivors) {
      auto it = std::find_if(params.lines.begin(), params.lines.end(),
                             [&](const SketchLine& l) { return l.id == id; });
      if (it == params.lines.end()) continue;
      it->constraint = plan.line_hv;
      it->constraint_driven = plan.line_hv_driven;
    }
  }

  // 2. Whitelisted relations onto every surviving line piece.
  for (const auto& [kind, partner] : plan.line_relations) {
    for (const auto& id : survivors) {
      if (id == partner) continue;
      if (relation_exists(params, kind, id, partner) ||
          relation_exists(params, kind, partner, id)) {
        continue;
      }
      params.line_relations.push_back(SketchLineRelation{
          .id = "rel-trim-" + kind + "-" + id + "-" + partner,
          .kind = kind,
          .first_line_id = id,
          .second_line_id = partner,
      });
    }
  }

  // 3. Concentric re-target onto surviving arcs (D4).
  for (const auto& other : plan.concentric_others) {
    for (const auto& id : survivors) {
      const bool survivor_is_arc = std::any_of(
          params.arcs.begin(), params.arcs.end(),
          [&](const SketchArc& a) { return a.id == id; });
      if (!survivor_is_arc) continue;
      if (pair_constraint_exists(params, "concentric", id, other)) continue;
      params.constraints.push_back(SketchConstraint{
          .constraint_id = next_constraint_id(params, "constraint-"),
          .kind = "concentric",
          .target_ids = {id, other},
      });
    }
  }

  // 4. Driven dimension re-derivation (measured, never stale drivers).
  for (const auto& id : survivors) {
    for (const auto& spec : plan.dim_specs) {
      // A circle trimmed to an arc reports its radius as arc_radius.
      const bool survivor_is_arc = std::any_of(
          params.arcs.begin(), params.arcs.end(),
          [&](const SketchArc& a) { return a.id == id; });
      const std::string kind = (spec.kind == "circle_radius" && survivor_is_arc)
                                   ? "arc_radius"
                                   : spec.kind;
      const std::string secondary =
          spec.secondary_entity_id.empty()
              ? ""
              : (entity_live(params, spec.secondary_entity_id)
                     ? spec.secondary_entity_id
                     : "");
      const bool secondary_required =
          kind == "angle" || kind == "circle_center_distance" ||
          kind == "circle_line_distance" || kind == "line_line_distance";
      if (secondary_required && secondary.empty()) continue;
      const auto value = transfer_dimension_value(params, kind, id, secondary);
      if (!value.has_value()) continue;
      params.dimensions.push_back(SketchDimension{
          .id = "dim-trim-" + kind + "-" + id,
          .kind = kind,
          .entity_id = id,
          .secondary_entity_id = secondary,
          .value = value.value(),
          .driven = true,
          .display_as = spec.display_as,
      });
    }
  }

  // 5. Point-on-object anchors for cut endpoints on LINE cutters.
  for (const auto& cut : cuts) {
    if (cut.cutter_entity_id.empty()) continue;
    const auto cutter = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const SketchLine& l) { return l.id == cut.cutter_entity_id; });
    if (cutter == params.lines.end()) continue;
    // An endpoint the cutter already owns needs no anchor (shared id
    // or coincident handles the junction).
    if (points_match(cut.x, cut.y, cutter->start_x, cutter->start_y) ||
        points_match(cut.x, cut.y, cutter->end_x, cutter->end_y)) {
      continue;
    }
    const double dx = cutter->end_x - cutter->start_x;
    const double dy = cutter->end_y - cutter->start_y;
    const double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) continue;
    const double t_raw = ((cut.x - cutter->start_x) * dx +
                          (cut.y - cutter->start_y) * dy) /
                         len2;
    // The cutter must actually CONTAIN the cut. The intersection was
    // computed against the cutter's pre-trim shape — a crossing-line
    // split may have shortened the cutter since (e.g. a circle trim
    // splits the line at the arc endpoints before the transfer runs).
    // A clamped projection would then drag the cut endpoint off the
    // cut onto the cutter's end — through a shared id that collapses
    // the neighbouring split piece to zero length.
    if (t_raw < -kTrimCoincidentTolerance ||
        t_raw > 1.0 + kTrimCoincidentTolerance) {
      continue;
    }
    const double proj_x = cutter->start_x + t_raw * dx;
    const double proj_y = cutter->start_y + t_raw * dy;
    if (std::hypot(cut.x - proj_x, cut.y - proj_y) >
        kTrimCoincidentTolerance) {
      continue;
    }
    const double t = std::clamp(t_raw, 0.0, 1.0);
    params.point_line_anchors.push_back(SketchVertexLineAnchor{
        .id = "anchor-trim-" + cut.point_id + "-" + cut.cutter_entity_id,
        .vertex_id = cut.point_id,
        .line_id = cut.cutter_entity_id,
        .t = t,
    });
  }


  // 6. Coincident re-application: the shared point must survive AND
  // every listed line must still reference it. The shared-id model
  // guarantees coincidence by construction — the positions need no
  // second check.
  const auto positions = live_sketch_point_positions(params);
  for (const auto& c : plan.coincident_snapshot) {
    if (!coincident_record_alive(params, c, positions)) continue;
    params.constraints.push_back(c);
  }
}
