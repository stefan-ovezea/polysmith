#pragma once

#include <array>
#include <optional>
#include <string>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

struct DocumentState;

// ══════════════════════════════════════════════════════════════════
//  Dimension resolution (P6) — the TNP ladder for drawing annotations
// ══════════════════════════════════════════════════════════════════
//
// Two entry paths share the measurement core:
//
//  • measure_from_picks() — preview/create: the user clicked on the
//    sheet; the click resolves to the nearest projected record (a
//    pick is never a stored ordinal, so re-projection cannot break
//    it — the TNP mantra).
//  • resolve_annotation() — the refresh pass: an annotation's stored
//    SourceEdgeWitness is matched against the fresh projection's
//    record sources (identity pass: body + edge index + kind — the
//    topology-stable fast path that follows parametric edits → strict
//    body+geometry pass → relaxed geometry pass → ambiguous → not
//    found).  On failure the annotation degrades with
//    dependency_broken + warning and keeps its last-known value —
//    never a silent substitute.
//
// Resolved attachments live in VIEW-mm (the projection plane at 1:1);
// the sheet transform (scale + position) is applied by the graphics
// builder.  Values are MODEL values (ISO 129-1: dimensions always
// read real sizes, independent of the view scale).

/// One resolved dimension attachment pair, ready for the graphics
/// builder.
struct ResolvedDimension {
  std::string kind = "linear";  // the annotation kind
  /// First attachment: for "line" a_p0/a_p1 are the endpoints; for
  /// "circle" a_p0 = center, a_p1 = the arc point nearest the pick
  /// (the leader direction), a_center/a_radius set.
  std::string a_kind = "line";
  std::array<double, 2> a_p0 = {0.0, 0.0};
  std::array<double, 2> a_p1 = {0.0, 0.0};
  std::optional<std::array<double, 2>> a_center;
  std::optional<double> a_radius;
  /// Second attachment (distance/angular between two edges).
  std::optional<std::array<double, 2>> b_p0;
  std::optional<std::array<double, 2>> b_p1;
  std::optional<std::array<double, 2>> b_center;
  std::optional<double> b_radius;
  /// The measured value: model-mm (or degrees for angular).
  double value = 0.0;
  /// Formatted display text (decimal separator, prefix/override
  /// applied).
  std::string text;
  bool broken = false;
  bool stale = false;  // last-known value shown (sources changed)
  std::string warning;
};

/// A pick (sheet-mm) resolved to the nearest projected record.
struct PickResolution {
  const ProjectedEdgeRecord* record = nullptr;
  /// Point on the record nearest the pick (view-mm).
  std::array<double, 2> nearest = {0.0, 0.0};
};

// ── Annotation attachments (GEOMETRY / SYMBOLS / ANNOTATE) ─────────

/// One resolved annotation attachment, ready for the graphics builder
/// — the annotation analogue of ResolvedDimension.  Annotation kinds
/// carry no measured value; their content is the user text.
struct ResolvedAttachment {
  std::string kind = "leader_text";
  /// First attachment: for "line" a_p0/a_p1 are the endpoints; for
  /// "circle" a_p0 = center, a_p1 = the arc point at attach_param,
  /// a_center/a_radius set.
  std::string a_kind = "line";
  std::array<double, 2> a_p0 = {0.0, 0.0};
  std::array<double, 2> a_p1 = {0.0, 0.0};
  std::optional<std::array<double, 2>> a_center;
  std::optional<double> a_radius;
  /// Second attachment (centerline between two circles).
  std::optional<std::array<double, 2>> b_p0;
  std::optional<std::array<double, 2>> b_p1;
  std::optional<std::array<double, 2>> b_center;
  std::optional<double> b_radius;
  /// The attachment point itself (the leader arrow / symbol anchor)
  /// — derived from attach_param, or the record start when the
  /// annotation has none.
  std::array<double, 2> attach_point = {0.0, 0.0};
  /// Display text (prefix + text_override).
  std::string text;
  bool broken = false;
  bool stale = false;  // last-known attachment shown (sources changed)
  std::string warning;
};

/// true when the kind belongs to the annotation family (leader_text,
/// center_mark, centerline, edge_extension, surface_finish, welding,
/// tolerance_frame, datum, balloon) rather than the dimension family.
bool is_annotation_kind(const AnnotationKind& kind);

/// The point on a record at a param fraction (0..1) over the
/// record's parameter span: lines lerp p_start→p_end; circle pieces
/// lerp start_angle→end_angle.
std::array<double, 2> point_at_param(const ProjectedEdgeRecord& rec,
                                     double fraction);

/// The param fraction (0..1) of a point along the record — the
/// inverse of point_at_param, clamped to [0,1].  Used at create time
/// to turn a pick into the stored attach_param.
double param_fraction_of(const ProjectedEdgeRecord& rec,
                         const std::array<double, 2>& point);

/// Formats a dimension value per ISO 129-1: rounded to 0.01, trailing
/// zeros stripped, decimal separator honored, "°" for angles.
std::string format_dimension_value(double value, bool angle,
                                   const std::string& decimal_separator);

/// Resolves one pick against the visible records of a projection.
/// `pick_viewmm` is in the view plane (model-mm at 1:1).  Returns
/// nullopt + fills `error` when nothing is near enough or the pick is
/// ambiguous (two records from different sources at the same place).
/// `prefer_witness_source` relaxes the ambiguity rule the annotation
/// way: when the coincident records differ only by source TYPE (one a
/// real edge witness, the other a silhouette attestation — e.g. a rim
/// circle in an axis view), the edge wins instead of refusing.  Two
/// real edges from different bodies still refuse.
std::optional<PickResolution> resolve_pick(const ProjectionResult& projection,
                                           const std::array<double, 2>& pick_viewmm,
                                           double tolerance_mm,
                                           bool prefer_witness_source,
                                           std::string* error);

/// Measures a freshly picked dimension (preview/create path): pick 1
/// required, pick 2 for distance/angular.  Kind semantics:
///   linear   — edge length, or distance between two picked edges
///   radius   — circle/arc radius
///   diameter — circle/arc diameter (ISO 129-1: arcs > 180° only)
///   angular  — angle between two picked lines
/// Returns nullopt + `error` on any validation failure (nothing is
/// mutated — the caller replies with the message).  `out_records`
/// receives the picked records (in pick order) so the create
/// mutator can mint witnesses from them.
std::optional<ResolvedDimension> measure_from_picks(
    const ProjectionResult& projection, const DrawingView& view,
    const AnnotationKind& dim_type,
    const std::array<double, 2>& pick_sheetmm,
    const std::optional<std::array<double, 2>>& pick_2_sheetmm,
    double pick_tolerance_sheetmm, const std::string& decimal_separator,
    const Annotation* annotation_for_text, std::string* error,
    std::vector<const ProjectedEdgeRecord*>* out_records = nullptr);

/// Resolves a stored annotation against a fresh projection (the
/// refresh path).  On success the measured value replaces the cached
/// one; on failure `broken` is set with a human-readable warning and
/// the caller keeps the last-known value (marked stale).
ResolvedDimension resolve_annotation(const ProjectionResult& projection,
                                     const Annotation& annotation,
                                     const std::string& decimal_separator);

/// Resolves a stored annotation of a NON-dimension kind against a
/// fresh projection (the refresh path) — the same witness ladder as
/// resolve_annotation, with the attachment point re-derived from
/// attach_param.  On failure `broken` is set with a warning and the
/// caller keeps the last-known attachment (marked stale).
ResolvedAttachment resolve_annotation_attachment(
    const ProjectionResult& projection, const Annotation& annotation,
    const std::string& decimal_separator);

/// Resolves fresh picks for a NON-dimension annotation (the
/// preview/create path, the annotation analogue of measure_from_picks):
/// pick 1 required, pick 2 for centerline.  Kind geometry validation
/// happens here (center_mark/centerline need circles, edge_extension
/// needs a line).  Returns nullopt + `error` on any failure (nothing
/// is mutated).  `out_records` receives the picked records (in pick
/// order) so the create mutator can mint witnesses; `out_attach_param`
/// receives the stored attachment fraction (unset for center_mark /
/// centerline, which anchor on the circle center).
std::optional<ResolvedAttachment> resolve_annotation_picks(
    const ProjectionResult& projection, const DrawingView& view,
    const AnnotationKind& kind,
    const std::array<double, 2>& pick_sheetmm,
    const std::optional<std::array<double, 2>>& pick_2_sheetmm,
    double pick_tolerance_sheetmm,
    const std::optional<std::string>& text_override,
    const std::string& prefix, std::string* error,
    std::vector<const ProjectedEdgeRecord*>* out_records = nullptr,
    std::optional<double>* out_attach_param = nullptr);

/// Mints the persistent edge reference id from a record's source.
/// Returns false + `error` when the record has no source edge witness
/// (silhouettes and cutting-plane traces cannot carry dimensions).
bool witness_from_record(const ProjectedEdgeRecord& record,
                         SourceEdgeWitness* out_witness, std::string* error);

}  // namespace polysmith::core
