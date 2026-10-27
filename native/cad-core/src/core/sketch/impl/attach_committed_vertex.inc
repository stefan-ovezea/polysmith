// Attach a freshly committed vertex to the geometry it was drawn onto.
//
// Fusion does this from the GEOMETRY: a line started on another line stays attached,
// whether or not a snap fired. Here the attachment lived in the UI's snap bookkeeping,
// so a lost flag left two lines that merely touched: a T-junction whose stem started on
// the host's midpoint had NO anchor at all ("midpoint_anchors": [] in the saved
// document), dragging the stem detached it and it stayed detached, and "Merge coincident
// points" had nothing to merge because nothing had ever been bound.
//
// So the core asks the question itself, at commit time, from where the vertex is: on a
// line's midpoint -> midpoint anchor; elsewhere along a line's body -> point-on-line
// anchor at that fraction.
//
// SCOPE MATTERS. Run unconditionally this broke four suites (trim, CAM, a
// partial-event test, auto-constrain) because those create lines through the same
// function and an anchor they did not ask for changes what their refresh does. It is
// therefore OPT-IN (`attach_to_geometry`), and only the IPC handler for a user-drawn
// line passes true; trim, import, projection, mirror, offset and CAM keep the default.
//
// Not covered yet: a vertex on a circle/arc boundary (needs point-on-circle, blocked by
// D-008) and a vertex at a settled intersection (needs two constraints; the candidate
// set does not carry the pair).

// How close a committed vertex has to be to a line's body to count as drawn onto it.
// The core coincidence tolerance is 0.01 mm; this is a PICK tolerance - a click that
// lands 0.2 mm off the line was aimed at the line.
constexpr double kCommittedAttachToleranceMm = 0.25;

// A vertex this close to the middle of its host IS the middle of it: 2 % of the host's
// length, capped so a long line cannot swallow a third of itself.
constexpr double kCommittedAttachMidpointFraction = 0.02;
constexpr double kCommittedAttachMidpointLimitMm = 2.0;

// Idempotent: does nothing when already attached, derived, or nothing within tolerance.
//
// `x`/`y` are passed in rather than read from `parameters.vertices` because the vertex
// table is REBUILT by the refresh: at commit time the new vertex is not in it yet, so a
// lookup would find nothing and the attachment would silently not happen. The table is
// still consulted first, so a post-snap position wins.
void attach_committed_vertex_to_geometry(SketchFeatureParameters& parameters,
                                         const std::string& vertex_id, double x,
                                         double y, double tolerance_mm) {
  if (vertex_id.rfind("vertex-text-", 0) == 0 ||
      vertex_id.rfind("vertex-slot-", 0) == 0) {
    return;
  }
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