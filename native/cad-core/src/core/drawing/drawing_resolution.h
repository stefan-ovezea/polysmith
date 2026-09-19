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

/// Formats a dimension value per ISO 129-1: rounded to 0.01, trailing
/// zeros stripped, decimal separator honored, "°" for angles.
std::string format_dimension_value(double value, bool angle,
                                   const std::string& decimal_separator);

/// Resolves one pick against the visible records of a projection.
/// `pick_viewmm` is in the view plane (model-mm at 1:1).  Returns
/// nullopt + fills `error` when nothing is near enough or the pick is
/// ambiguous (two records from different sources at the same place).
std::optional<PickResolution> resolve_pick(const ProjectionResult& projection,
                                           const std::array<double, 2>& pick_viewmm,
                                           double tolerance_mm,
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

/// Mints the persistent edge reference id from a record's source.
/// Returns false + `error` when the record has no source edge witness
/// (silhouettes and cutting-plane traces cannot carry dimensions).
bool witness_from_record(const ProjectedEdgeRecord& record,
                         SourceEdgeWitness* out_witness, std::string* error);

}  // namespace polysmith::core
