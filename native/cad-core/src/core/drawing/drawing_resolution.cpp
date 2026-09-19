#include "core/drawing/drawing_resolution.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

// ── 2D point math (view plane, model-mm) ──────────────────────────

double dist2(const std::array<double, 2>& a, const std::array<double, 2>& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  return std::sqrt(dx * dx + dy * dy);
}

double dot2(const std::array<double, 2>& a, const std::array<double, 2>& b) {
  return a[0] * b[0] + a[1] * b[1];
}

double cross2(const std::array<double, 2>& a, const std::array<double, 2>& b) {
  return a[0] * b[1] - a[1] * b[0];
}

// ── 3D point math (witness matching) ──────────────────────────────

double dist3(const std::array<double, 3>& a, const std::array<double, 3>& b) {
  const double dx = a[0] - b[0];
  const double dy = a[1] - b[1];
  const double dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ── Record geometry queries ───────────────────────────────────────

/// Distance from a point to a record's curve (view-mm): segment
/// distance for lines, |dist-to-center − r| for circles, sampled
/// (48 points) for ellipses.  Records without drawable geometry are
/// skipped by the caller.
double point_record_distance(const ProjectedEdgeRecord& rec,
                             const std::array<double, 2>& p) {
  if (rec.curve_kind == "circle" && rec.circle_center.has_value() &&
      rec.circle_radius.has_value()) {
    return std::abs(dist2(p, rec.circle_center.value()) -
                    rec.circle_radius.value());
  }
  if (rec.curve_kind == "ellipse" && rec.ellipse_center.has_value() &&
      rec.ellipse_major_dir.has_value() && rec.ellipse_major_radius.has_value() &&
      rec.ellipse_minor_radius.has_value()) {
    // Sample the ellipse piece (start..end angle) — nearest sampled
    // point.  48 segments is the same tessellation the hatcher uses.
    const auto& c = rec.ellipse_center.value();
    const double ma = rec.ellipse_major_radius.value();
    const double mi = rec.ellipse_minor_radius.value();
    const auto& md = rec.ellipse_major_dir.value();
    const double n0 = -md[1];
    const double n1 = md[0];
    const double a0 = rec.start_angle;
    const double a1 = rec.end_angle;
    double best = 1e300;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i) {
      const double a = a0 + (a1 - a0) * static_cast<double>(i) / steps;
      const double x = c[0] + md[0] * ma * std::cos(a) + n0 * mi * std::sin(a);
      const double y = c[1] + md[1] * ma * std::cos(a) + n1 * mi * std::sin(a);
      best = std::min(best, dist2(p, {x, y}));
    }
    return best;
  }
  // Lines (and anything else): point-to-segment distance.
  const auto& a = rec.p_start;
  const auto& b = rec.p_end;
  const std::array<double, 2> ab = {b[0] - a[0], b[1] - a[1]};
  const double len2 = dot2(ab, ab);
  if (len2 < 1e-18) {
    return dist2(p, a);
  }
  double t = dot2({p[0] - a[0], p[1] - a[1]}, ab) / len2;
  t = std::clamp(t, 0.0, 1.0);
  return dist2(p, {a[0] + t * ab[0], a[1] + t * ab[1]});
}

/// The point on the record nearest `p` (clamped for lines).
std::array<double, 2> nearest_point_on_record(const ProjectedEdgeRecord& rec,
                                              const std::array<double, 2>& p) {
  if (rec.curve_kind == "circle" && rec.circle_center.has_value() &&
      rec.circle_radius.has_value()) {
    const auto& c = rec.circle_center.value();
    const double dx = p[0] - c[0];
    const double dy = p[1] - c[1];
    const double d = std::sqrt(dx * dx + dy * dy);
    if (d < 1e-12) {
      return {c[0] + rec.circle_radius.value(), c[1]};
    }
    return {c[0] + dx / d * rec.circle_radius.value(),
            c[1] + dy / d * rec.circle_radius.value()};
  }
  const auto& a = rec.p_start;
  const auto& b = rec.p_end;
  const std::array<double, 2> ab = {b[0] - a[0], b[1] - a[1]};
  const double len2 = dot2(ab, ab);
  if (len2 < 1e-18) {
    return a;
  }
  double t = dot2({p[0] - a[0], p[1] - a[1]}, ab) / len2;
  t = std::clamp(t, 0.0, 1.0);
  return {a[0] + t * ab[0], a[1] + t * ab[1]};
}

/// true when a record is a dimension candidate: visible, drawable,
/// not a cutting-plane trace.
bool is_pickable(const ProjectedEdgeRecord& rec) {
  if (rec.line_class != "visible") {
    return false;
  }
  if (rec.curve_class == "smooth" || rec.curve_class == "cutting_plane") {
    return false;
  }
  return true;
}

/// A record's source identity — two records with the same identity
/// are pieces of the SAME source edge (HLR splits edges).  The
/// geometry fallback requires the same body: coincident edges of
/// DIFFERENT bodies are distinct sources (the ambiguity rule).
bool same_source(const ProjectedEdgeRecord& a, const ProjectedEdgeRecord& b) {
  const auto* wa = std::get_if<SourceEdgeWitness>(&a.source);
  const auto* wb = std::get_if<SourceEdgeWitness>(&b.source);
  if (wa == nullptr || wb == nullptr) {
    return false;
  }
  if (!wa->body_id.empty() && wa->body_id == wb->body_id &&
      wa->src_edge_index >= 0 && wa->src_edge_index == wb->src_edge_index) {
    return true;
  }
  if (wa->body_id.empty() || wa->body_id != wb->body_id) {
    return false;
  }
  // Fallback (same body, no index): identical witness geometry
  // (endpoints or center/radius).
  if (wa->curve_kind == wb->curve_kind) {
    if (wa->curve_kind == "circle" || wa->curve_kind == "ellipse") {
      return wa->center.has_value() && wb->center.has_value() &&
             wa->radius.has_value() && wb->radius.has_value() &&
             dist3(wa->center.value(), wb->center.value()) < 1e-3 &&
             std::abs(wa->radius.value() - wb->radius.value()) < 1e-3;
    }
    const bool same_direction =
        dist3(wa->start_point, wb->start_point) < 1e-3 &&
        dist3(wa->end_point, wb->end_point) < 1e-3;
    const bool flipped =
        dist3(wa->start_point, wb->end_point) < 1e-3 &&
        dist3(wa->end_point, wb->start_point) < 1e-3;
    return same_direction || flipped;
  }
  return false;
}

/// Witness match with a geometry tolerance (mm) — the body id must
/// match exactly (the strict pass); the relaxed pass reuses this with
/// `require_body = false`.
bool witness_matches(const SourceEdgeWitness& stored,
                     const SourceEdgeWitness& record, double tol,
                     bool require_body) {
  if (require_body && stored.body_id != record.body_id) {
    return false;
  }
  if (stored.curve_kind != record.curve_kind) {
    return false;
  }
  if (stored.curve_kind == "circle" || stored.curve_kind == "ellipse") {
    return stored.center.has_value() && record.center.has_value() &&
           stored.radius.has_value() && record.radius.has_value() &&
           dist3(stored.center.value(), record.center.value()) <= tol &&
           std::abs(stored.radius.value() - record.radius.value()) <= tol;
  }
  const bool same_direction =
      dist3(stored.start_point, record.start_point) <= tol &&
      dist3(stored.end_point, record.end_point) <= tol;
  const bool flipped = dist3(stored.start_point, record.end_point) <= tol &&
                       dist3(stored.end_point, record.start_point) <= tol;
  return same_direction || flipped;
}

/// Arc sweep of a record (radians); full circles store start == end
/// and count as 2π.
double record_sweep(const ProjectedEdgeRecord& rec) {
  const double sweep = std::abs(rec.end_angle - rec.start_angle);
  return sweep < 1e-6 ? 2.0 * kPi : sweep;
}

/// Unit direction of a line record.
std::array<double, 2> line_direction(const ProjectedEdgeRecord& rec) {
  const double dx = rec.p_end[0] - rec.p_start[0];
  const double dy = rec.p_end[1] - rec.p_start[1];
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-12) {
    return {1.0, 0.0};
  }
  return {dx / len, dy / len};
}

// ── Measurement ───────────────────────────────────────────────────

/// Measures record geometry into a ResolvedDimension (view-mm
/// attachments + model value).  Returns false + `error` on
/// unsupported combinations (nothing is mutated).
bool measure_records(const AnnotationKind& kind, const ProjectedEdgeRecord* rec1,
                     const std::array<double, 2>& nearest1,
                     const ProjectedEdgeRecord* rec2,
                     const std::array<double, 2>& nearest2,
                     ResolvedDimension* out, std::string* error) {
  out->kind = kind;
  const auto attach_line = [&](const ProjectedEdgeRecord& rec,
                               const std::array<double, 2>& nearest,
                               std::array<double, 2>* p0, std::array<double, 2>* p1,
                               std::optional<std::array<double, 2>>* center,
                               std::optional<double>* radius) {
    if (rec.curve_kind == "circle" && rec.circle_center.has_value() &&
        rec.circle_radius.has_value()) {
      *p0 = rec.circle_center.value();
      *p1 = nearest;
      *center = rec.circle_center;
      *radius = rec.circle_radius;
      return "circle";
    }
    *p0 = rec.p_start;
    *p1 = rec.p_end;
    return "line";
  };
  const std::string a_kind = attach_line(*rec1, nearest1, &out->a_p0, &out->a_p1,
                                         &out->a_center, &out->a_radius);
  out->a_kind = a_kind;

  if (rec2 != nullptr) {
    std::array<double, 2> b0;
    std::array<double, 2> b1;
    std::optional<std::array<double, 2>> b_center;
    std::optional<double> b_radius;
    const std::string b_kind =
        attach_line(*rec2, nearest2, &b0, &b1, &b_center, &b_radius);
    out->b_p0 = b0;
    out->b_p1 = b1;
    out->b_center = b_center;
    out->b_radius = b_radius;

    if (kind == "angular") {
      if (a_kind != "line" || b_kind != "line") {
        *error = "Angle dimensions need two straight edges.";
        return false;
      }
      const auto d1 = line_direction(*rec1);
      const auto d2 = line_direction(*rec2);
      const double dot = std::clamp(dot2(d1, d2), -1.0, 1.0);
      const double parallel = std::abs(cross2(d1, d2));
      if (parallel < 0.087) {  // < 5°: no measurable angle
        *error = "The picked edges are parallel — use a distance dimension.";
        return false;
      }
      // ISO 129-1 shows the smaller sector between the two lines.
      const double angle = std::acos(std::abs(dot));
      out->value = angle * 180.0 / kPi;
      return true;
    }
    if (kind == "linear") {
      if (a_kind == "circle" && b_kind == "circle") {
        // For circle attachments a_p0/b_p0 ARE the centers.
        out->value = dist2(out->a_p0, out->b_p0.value());
        return true;
      }
      if (a_kind == "line" && b_kind == "line") {
        const auto d = line_direction(*rec1);
        const double parallel = std::abs(cross2(d, line_direction(*rec2)));
        if (parallel >= 0.087) {
          *error =
              "The picked edges are not parallel — use an angle dimension.";
          return false;
        }
        // Perpendicular distance between the two infinite lines.
        const auto& p = rec1->p_start;
        const auto& q = rec2->p_start;
        const double dx = q[0] - p[0];
        const double dy = q[1] - p[1];
        out->value = std::abs(cross2(d, {dx, dy}));
        return true;
      }
      // line + circle: perpendicular distance from the center to the
      // line.
      const ProjectedEdgeRecord* line_rec = a_kind == "line" ? rec1 : rec2;
      const ProjectedEdgeRecord* circle_rec = a_kind == "circle" ? rec1 : rec2;
      const auto d = line_direction(*line_rec);
      const auto& c = circle_rec->circle_center.value();
      const auto& p = line_rec->p_start;
      const double dx = c[0] - p[0];
      const double dy = c[1] - p[1];
      out->value = std::abs(cross2(d, {dx, dy}));
      return true;
    }
    *error = "This dimension kind does not take a second edge.";
    return false;
  }

  if (kind == "linear") {
    if (a_kind != "line") {
      *error =
          "A linear dimension needs a straight edge — use radius or "
          "diameter for circles.";
      return false;
    }
    out->value = dist2(rec1->p_start, rec1->p_end);
    return true;
  }
  if (kind == "radius" || kind == "diameter") {
    if (a_kind != "circle") {
      *error =
          (kind == "radius" ? "A radius" : "A diameter") +
          std::string(" dimension needs a circle or arc.") ;
      return false;
    }
    if (kind == "diameter" && record_sweep(*rec1) <= kPi + 1e-6) {
      // ISO 129-1: diameter dimensions are for arcs > 180°; smaller
      // arcs take a radius.
      *error =
          "Diameter dimensions apply to arcs larger than a semicircle "
          "— use a radius dimension.";
      return false;
    }
    out->value =
        out->a_radius.value() * (kind == "diameter" ? 2.0 : 1.0);
    return true;
  }
  if (kind == "angular") {
    *error = "Angle dimensions need two straight edges.";
    return false;
  }
  *error = "Unknown dimension kind '" + kind + "'.";
  return false;
}

}  // namespace

// ── Formatting ────────────────────────────────────────────────────

std::string format_dimension_value(double value, bool angle,
                                   const std::string& decimal_separator) {
  if (std::abs(value) < 0.005) {
    value = 0.0;
  }
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << value;
  std::string s = oss.str();
  // Strip trailing zeros, then a dangling decimal point.
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0') {
      s.pop_back();
    }
    if (!s.empty() && s.back() == '.') {
      s.pop_back();
    }
  }
  if (decimal_separator != ".") {
    const auto dot = s.find('.');
    if (dot != std::string::npos) {
      s[dot] = decimal_separator[0];
    }
  }
  if (angle) {
    s += "\xC2\xB0";  // ° (UTF-8) — angles always carry the unit
  }
  return s;
}

// ── Pick resolution ───────────────────────────────────────────────

std::optional<PickResolution> resolve_pick(const ProjectionResult& projection,
                                           const std::array<double, 2>& pick_viewmm,
                                           double tolerance_mm,
                                           std::string* error) {
  const ProjectedEdgeRecord* best = nullptr;
  double best_dist = 1e300;
  double second_dist = 1e300;
  const ProjectedEdgeRecord* second = nullptr;
  for (const auto& rec : projection.edges) {
    if (!is_pickable(rec)) {
      continue;
    }
    const double d = point_record_distance(rec, pick_viewmm);
    if (d < best_dist) {
      second_dist = best_dist;
      second = best;
      best_dist = d;
      best = &rec;
    } else if (d < second_dist) {
      second_dist = d;
      second = &rec;
    }
  }
  if (best == nullptr || best_dist > tolerance_mm) {
    *error = "No edge near the pick — click on an edge of the view.";
    return std::nullopt;
  }
  // Two coincident edges from different sources: the pick cannot tell
  // them apart — refuse rather than guess (never silently substitute).
  if (second != nullptr && second_dist <= best_dist + 1e-3 &&
      !same_source(*best, *second)) {
    *error = "The pick is ambiguous — several coincident edges overlap here.";
    return std::nullopt;
  }
  return PickResolution{best, nearest_point_on_record(*best, pick_viewmm)};
}

// ── Preview/create path ───────────────────────────────────────────

bool witness_from_record(const ProjectedEdgeRecord& record,
                         SourceEdgeWitness* out_witness, std::string* error) {
  const auto* witness = std::get_if<SourceEdgeWitness>(&record.source);
  if (witness == nullptr || witness->body_id.empty()) {
    *error =
        "This edge has no stable model reference (a silhouette or "
        "cutting-plane line) — dimensions cannot attach to it.";
    return false;
  }
  *out_witness = *witness;
  return true;
}

std::optional<ResolvedDimension> measure_from_picks(
    const ProjectionResult& projection, const DrawingView& view,
    const AnnotationKind& dim_type,
    const std::array<double, 2>& pick_sheetmm,
    const std::optional<std::array<double, 2>>& pick_2_sheetmm,
    double pick_tolerance_sheetmm, const std::string& decimal_separator,
    const Annotation* annotation_for_text, std::string* error,
    std::vector<const ProjectedEdgeRecord*>* out_records) {
  const double s = view.scale;
  const auto to_view = [&](const std::array<double, 2>& p) {
    return std::array<double, 2>{
        (p[0] - view.sheet_position[0]) / s,
        (p[1] - view.sheet_position[1]) / s};
  };
  const auto pick1 = resolve_pick(projection, to_view(pick_sheetmm),
                                  pick_tolerance_sheetmm / s, error);
  if (!pick1.has_value()) {
    return std::nullopt;
  }
  std::optional<PickResolution> pick2;
  if (pick_2_sheetmm.has_value()) {
    pick2 = resolve_pick(projection, to_view(pick_2_sheetmm.value()),
                         pick_tolerance_sheetmm / s, error);
    if (!pick2.has_value()) {
      return std::nullopt;
    }
    if (same_source(*pick1->record, *pick2->record)) {
      *error = "Pick two different edges for the second attachment.";
      return std::nullopt;
    }
  }
  if (out_records != nullptr) {
    out_records->clear();
    out_records->push_back(pick1->record);
    if (pick2.has_value()) {
      out_records->push_back(pick2->record);
    }
  }

  ResolvedDimension resolved;
  if (!measure_records(dim_type, pick1->record, pick1->nearest,
                       pick2.has_value() ? pick2->record : nullptr,
                       pick2.has_value() ? pick2->nearest
                                         : std::array<double, 2>{0.0, 0.0},
                       &resolved, error)) {
    return std::nullopt;
  }

  // Display text: defaults per kind (⌀ diameter, R radius), the
  // annotation's cosmetic fields win for update/preview re-renders.
  const bool angle = resolved.kind == "angular";
  std::string prefix;
  std::optional<std::string> override_text;
  if (annotation_for_text != nullptr) {
    prefix = annotation_for_text->prefix;
    override_text = annotation_for_text->text_override;
  } else if (resolved.kind == "diameter") {
    prefix = "\xE2\x8C\x80";  // ⌀
  } else if (resolved.kind == "radius") {
    prefix = "R";
  }
  if (override_text.has_value() && !override_text.value().empty()) {
    resolved.text = prefix + override_text.value();
  } else {
    resolved.text = prefix + format_dimension_value(resolved.value, angle,
                                                    decimal_separator);
  }
  return resolved;
}

// ── Refresh path (the witness ladder) ─────────────────────────────

ResolvedDimension resolve_annotation(const ProjectionResult& projection,
                                     const Annotation& annotation,
                                     const std::string& decimal_separator) {
  ResolvedDimension out;
  out.kind = annotation.kind;
  out.broken = true;

  // Ladder rung 1 (identity): same body + same edge index + same
  // kind.  The index is minted through the same deterministic compile
  // pipeline (TopExp::MapShapes order), so a parametric edit (resize)
  // keeps it stable — the dimension follows the model.  Rung 2:
  // strict geometry + body within 0.01 mm.  Rung 3: relaxed geometry
  // within 0.1 mm (the feature was re-created — a fillet broke the
  // body id).  Rung 4: not found → broken.
  auto find_candidates = [&](const SourceEdgeWitness& target,
                             bool require_body, double tol) {
    std::vector<const ProjectedEdgeRecord*> matches;
    for (const auto& rec : projection.edges) {
      if (rec.line_class != "visible" || rec.curve_class == "smooth" ||
          rec.curve_class == "cutting_plane") {
        continue;
      }
      const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
      if (witness == nullptr || witness->body_id.empty()) {
        continue;
      }
      if (witness_matches(target, *witness, tol, require_body)) {
        matches.push_back(&rec);
      }
    }
    return matches;
  };

  auto resolve_one = [&](const SourceEdgeWitness& target,
                         std::vector<const ProjectedEdgeRecord*>* out_matches,
                         std::string* warning) {
    // Identity pass: body + edge index + kind (topology-stable).
    if (!target.body_id.empty() && target.src_edge_index >= 0) {
      for (const auto& rec : projection.edges) {
        if (rec.line_class != "visible" || rec.curve_class == "smooth" ||
            rec.curve_class == "cutting_plane") {
          continue;
        }
        const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
        if (witness != nullptr &&
            witness->body_id == target.body_id &&
            witness->src_edge_index == target.src_edge_index &&
            witness->curve_kind == target.curve_kind) {
          out_matches->push_back(&rec);
        }
      }
    }
    if (out_matches->empty()) {
      *out_matches = find_candidates(target, /*require_body=*/true, 0.01);
    }
    if (out_matches->empty()) {
      *out_matches = find_candidates(target, /*require_body=*/false, 0.1);
    }
    if (out_matches->empty()) {
      *warning = "The edge referenced by this dimension was not found — "
                 "the dimension shows its last-known value.";
      return false;
    }
    // Distinct sources among the candidates → ambiguous; records from
    // the SAME source are pieces of one HLR-split edge and merge.
    const ProjectedEdgeRecord* first = out_matches->front();
    for (const auto* other : *out_matches) {
      if (!same_source(*first, *other)) {
        *warning =
            "The dimension's edge is ambiguous after the model changed — "
            "the dimension shows its last-known value.";
        return false;
      }
    }
    // Longest piece of the split edge wins (the dominant span).
    const ProjectedEdgeRecord* best = first;
    for (const auto* other : *out_matches) {
      const double other_span = dist2(other->p_start, other->p_end) +
                                (other->circle_radius.has_value()
                                     ? 2.0 * kPi * other->circle_radius.value()
                                     : 0.0);
      const double best_span = dist2(best->p_start, best->p_end) +
                               (best->circle_radius.has_value()
                                    ? 2.0 * kPi * best->circle_radius.value()
                                    : 0.0);
      if (other_span > best_span) {
        best = other;
      }
    }
    out_matches->assign(1, best);
    return true;
  };

  std::vector<const ProjectedEdgeRecord*> matches1;
  if (!resolve_one(annotation.witness, &matches1, &out.warning)) {
    return out;
  }
  const ProjectedEdgeRecord* rec1 = matches1[0];
  std::array<double, 2> nearest1 =
      nearest_point_on_record(*rec1, rec1->p_start);

  const ProjectedEdgeRecord* rec2 = nullptr;
  std::array<double, 2> nearest2 = {0.0, 0.0};
  if (annotation.witness_2.has_value()) {
    std::vector<const ProjectedEdgeRecord*> matches2;
    if (!resolve_one(annotation.witness_2.value(), &matches2, &out.warning)) {
      return out;
    }
    rec2 = matches2[0];
    nearest2 = nearest_point_on_record(*rec2, rec2->p_start);
  }

  std::string error;
  if (!measure_records(annotation.kind, rec1, nearest1, rec2, nearest2, &out,
                       &error)) {
    out.broken = true;
    out.warning = error;
    return out;
  }
  const bool angle = out.kind == "angular";
  if (annotation.text_override.has_value() &&
      !annotation.text_override.value().empty()) {
    out.text = annotation.prefix + annotation.text_override.value();
  } else {
    out.text = annotation.prefix +
               format_dimension_value(out.value, angle, decimal_separator);
  }
  out.broken = false;
  return out;
}

}  // namespace polysmith::core
