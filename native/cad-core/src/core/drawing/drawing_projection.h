#pragma once

#include <optional>
#include <string>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

// ── Drawing projection engine (P2, promoted from the P0 spike) ────
//
// `project()` is a pure function: compiled source shapes + a resolved
// view frame → a complete immutable ProjectionResult.  The result is
// cached by the refresh pass in drawing_runtime (revision-keyed) and
// never serialized.
//
// Architecture (binding, proven by cad_core_hlr_provenance_test):
// iterate HLRBRep_InternalAlgo's public data structure DIRECTLY —
// each entry yields (source edge via EdgeMap, HLRAlgo_EdgeStatus
// visible intervals, HLRBRep_Curve projected geometry).  Records are
// built with HLRBRep::MakeEdge(entry curve, interval) per visible
// part, so provenance is BORN with the record and no output↔entry
// matching heuristics exist at all.  Silhouettes come from
// OutLineVCompound (the face-wire path — outline curves are not edge
// entries) and carry a body-level FaceAttestation.

/// One source body for a view: the compiled shape + the body id the
/// witness references (the id the TNP resolution re-resolves against
/// on every recompute).
struct SourceBody {
  std::string body_id;
  TopoDS_Shape shape;
};

struct ProjectionInput {
  std::vector<SourceBody> sources;
  DrawingViewFrame frame;
  bool show_hidden = false;
  /// Document revision the source shapes were compiled at — stamped
  /// onto the result by the refresh pass.
  int source_revision = -1;
};

/// Projects the sources along the frame.  Output records are
/// deterministically ordered (line class rank, curve class, geometry
/// signature, body id, edge index) — the golden-file foundation.
ProjectionResult project(const ProjectionInput& input);

/// Matches a mapped HLR source edge against the edges of a compiled
/// body and returns its index in the body's edge map (0-based, the
/// capture_edge_reference convention).  HLR may SPLIT source edges
/// (a full circle rim becomes silhouette-bounded arcs whose TShapes
/// are copies), so the match is IsSame first, then curve-geometry
/// equality within tolerance (same line / circle / ellipse).  Returns
/// -1 when nothing matches (BSpline copies are not matched).
int match_body_edge(const TopoDS_Shape& body_shape,
                    const TopoDS_Edge& mapped_edge);

/// Builds a TNP-safe witness for the body edge at `edge_index` in
/// the body's edge map.  Circle edges additionally carry
/// center/axis/radius (full circles AND arcs — arcs keep their
/// endpoints).  Returns nullopt when the index is out of range.
std::optional<SourceEdgeWitness> build_source_edge_witness(
    const std::string& body_id, const TopoDS_Shape& body_shape,
    int edge_index);

/// Resolves an ISO standard view name to a frame.  Convention
/// (documented, pinned by the projection test):
///   - world Z is up; "front" is the +X face (the P0 spike frame).
///   - side views (front/right/left/back) keep view-Y = normal ×
///     x_direction = +Z — the part draws upright.
///   - top/bottom keep view-Y = -X — first-angle layout (ISO 128-3):
///     the part's back (-X) faces up on the sheet, so a top view
///     placed below the front view lines up with it.
///   front:  N ( 1, 0, 0), X ( 0, 1, 0)
///   right:  N ( 0, 1, 0), X (-1, 0, 0)
///   left:   N ( 0,-1, 0), X ( 1, 0, 0)
///   top:    N ( 0, 0, 1), X ( 0, 1, 0)
///   bottom: N ( 0, 0,-1), X ( 0,-1, 0)
///   back:   N (-1, 0, 0), X ( 0,-1, 0)
/// Returns nullopt for unknown names.
std::optional<DrawingViewFrame> standard_view_frame(
    const std::string& standard_view);

}  // namespace polysmith::core
