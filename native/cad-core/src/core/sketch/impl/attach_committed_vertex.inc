// Attach a freshly committed vertex to the geometry it was drawn onto.
//
// Fusion does this from the GEOMETRY: a line started on another line stays attached,
// whether or not a snap fired. Here the attachment lived in the UI's snap bookkeeping,
// so a lost flag left two lines that merely touched: a T-junction whose stem started on
// the host's midpoint had NO anchor at all ("midpoint_anchors": [] in the saved
// document), dragging the stem detached it and it stayed detached, and "Merge coincident
// points" had nothing to merge because nothing had ever been bound.
//
// The core therefore asks the question itself at commit time, from where the vertex is:
// on a line's midpoint -> midpoint anchor; elsewhere along a line's body -> point-on-line
// anchor at that fraction.
//
// SCOPE MATTERS. Run unconditionally this broke four suites (trim, CAM, a partial-event
// test, auto-constrain): they create lines through the same function and an anchor they
// did not ask for changes what their refresh does. It is OPT-IN
// (`attach_to_geometry`), and only the IPC handler for a user-drawn line passes true.
//
// NOT covered: a circle's or an arc's boundary. The constraint exists (planegcs
// PointOnCircle, mapped and verified - see solver_solution_check.h) but it does not move
// the point yet after a circle edit, so attaching would ship a constraint that does
// nothing, which is exactly what D-008 warns about. Investigated and recorded there.
//
// An arc's boundary additionally needs the sweep: PointOnCircle knows only the radius,
// so the point could legally sit on the half the arc does not cover. Not covered yet: an arc's boundary (PointOnCircle knows only the radius, not the
// sweep, so the point could jump to the half the arc does not cover) and a settled
// intersection (needs two constraints; the candidate set does not carry the pair).

// How close a committed vertex has to be to count as drawn onto something. The core
// coincidence tolerance is 0.01 mm; this is a PICK tolerance - a click 0.2 mm off the
// line was aimed at the line.
constexpr double kCommittedAttachToleranceMm = 0.25;

// A vertex this close to the middle of its host IS the middle of it: 2 % of the host's
// length, capped so a long line cannot swallow a third of itself.
constexpr double kCommittedAttachMidpointFraction = 0.02;
constexpr double kCommittedAttachMidpointLimitMm = 2.0;

// Vertices owned by an entity and pinned by it. The circle/arc quadrant and centre
// points matter most: a line started exactly on a quadrant REUSES that vertex (endpoint
// sharing), so it is already tied to the circle - adding point_on_circle on top
// constrains a FIXED point, which is unsatisfiable and made the verifier reject every
// solve of that sketch ("point_on_circle vertex-circle-1-quadrant-0/circle-1 off by
// 38.97 mm", after which the whole geometry froze).
bool vertex_is_derived(const std::string& vertex_id) {
  static constexpr const char* kPrefixes[] = {
      "vertex-text-", "vertex-slot-", "vertex-circle-", "vertex-arc-",
      "vertex-ellipse-", "vertex-spline-"};
  for (const char* prefix : kPrefixes) {
    if (vertex_id.rfind(prefix, 0) == 0) return true;
  }
  return false;
}

// Idempotent: does nothing when already attached, derived, or nothing within tolerance.
//
// `x`/`y` are passed in rather than read from `parameters.vertices`: that table is
// REBUILT by the refresh, so at commit time the new vertex is not in it yet and a
// lookup would find nothing (the first version did exactly that and silently did
// nothing). The table is still consulted first, so a post-snap position wins.
void attach_committed_vertex_to_geometry(SketchFeatureParameters& parameters,
                                         const std::string& vertex_id, double x,
                                         double y, double tolerance_mm) {
  if (vertex_is_derived(vertex_id)) return;
  for (const auto& candidate : parameters.vertices) {
    if (candidate.id == vertex_id) {
      x = candidate.x;
      y = candidate.y;
      break;
    }
  }
  for (const auto& anchor : parameters.midpoint_anchors) {
    if (anchor.vertex_id == vertex_id) return;
  }
  for (const auto& anchor : parameters.point_line_anchors) {
    if (anchor.vertex_id == vertex_id) return;
  }

  const SketchLine* host = nullptr;
  double host_distance = tolerance_mm;
  double host_t = 0.0;
  double host_length = 0.0;
  for (const auto& line : parameters.lines) {
    if (line.start_vertex_id == vertex_id || line.end_vertex_id == vertex_id) {
      continue;  // a vertex cannot be attached to its own line
    }
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= kMinimumSketchDimensionValue) continue;
    const double t =
        ((x - line.start_x) * dx + (y - line.start_y) * dy) / (length * length);
    // Only the segment itself, with a hair of slack for a click just past an end.
    // Beyond the ends the nearest point is an endpoint, and endpoint sharing handled it.
    if (t < -1e-3 || t > 1.0 + 1e-3) continue;
    const double foot_x = line.start_x + t * dx;
    const double foot_y = line.start_y + t * dy;
    const double distance = std::hypot(x - foot_x, y - foot_y);
    if (distance <= host_distance) {
      host_distance = distance;
      host = &line;
      host_t = std::clamp(t, 0.0, 1.0);
      host_length = length;
    }
  }

  if (host == nullptr) return;

  const double midpoint_band =
      std::max(std::min(kCommittedAttachMidpointFraction * host_length,
                        kCommittedAttachMidpointLimitMm),
               kMinimumSketchDimensionValue) /
      host_length;

  if (std::abs(host_t - 0.5) <= midpoint_band) {
    parameters.midpoint_anchors.push_back(SketchMidpointAnchor{
        .id = "midpoint-anchor-" +
              std::to_string(parameters.midpoint_anchors.size() + 1),
        .vertex_id = vertex_id,
        .line_id = host->id,
    });
    log_info("sketch", "attached " + vertex_id + " to the midpoint of " +
                           host->id + " (from the geometry, not from a snap)");
    return;
  }

  parameters.point_line_anchors.push_back(SketchVertexLineAnchor{
      .id = "point-line-anchor-" +
            std::to_string(parameters.point_line_anchors.size() + 1),
      .vertex_id = vertex_id,
      .line_id = host->id,
      .t = host_t,
  });
  log_info("sketch", "attached " + vertex_id + " to " + host->id + " at t=" +
                         std::to_string(host_t) +
                         " (from the geometry, not from a snap)");
}