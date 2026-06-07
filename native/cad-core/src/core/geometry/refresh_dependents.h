#pragma once

#include <optional>
#include <string>

#include "core/construction_plane_feature.h"
#include "core/feature.h"

namespace polysmith::core {

struct DocumentState;

// Resolve the world-space frame of any "selectable plane" source the
// rest of the codebase deals with:
//
//   * "ref-plane-xy", "ref-plane-yz", "ref-plane-xz" — origin
//     reference planes. The frames are constant.
//   * "feature-N" — a construction-plane feature in `document`. We
//     return its already-cached `plane_frame`. Callers running inside
//     `refresh_history_dependencies` should make sure the source
//     construction plane has been refreshed earlier in the same pass
//     before calling this — the topological walk does that
//     naturally.
//   * "<body_id>:face:<index>" — a planar face on a body. We compile
//     the bodies from `document` and pull the face's plane out of
//     OCCT, mirroring the path face-based sketches use.
//   * sketch profile ids — the owning sketch plane, with the frame
//     origin centered on the profile region.
//
// Returns nullopt when the id doesn't match any supported form,
// when the upstream feature / face is missing, or when the resolved
// face is non-planar.
std::optional<PlaneFrame> resolve_plane_source_frame(
    const DocumentState& document,
    const std::string& source_id);

// Resolve a tangent frame for a body face id. Unlike
// `resolve_plane_source_frame`, this intentionally accepts non-planar
// faces and samples a representative point/normal from the surface.
std::optional<PlaneFrame> resolve_tangent_plane_source_frame(
    const DocumentState& document,
    const std::string& source_id);

// Resolve a linear axis for tools such as Plane at Angle. Supported
// sources are body line edges (`<body_id>:edge:<index>`) and sketch
// line ids. Both are re-resolved from the current document state so
// downstream construction planes degrade instead of holding stale
// topology when upstream geometry changes.
std::optional<ConstructionAxisFrame> resolve_angle_plane_axis(
    const DocumentState& document,
    const std::string& source_id);

std::optional<ConstructionAxisFeatureParameters> resolve_construction_axis_source(
    const DocumentState& document,
    const std::string& source_id);

std::optional<ConstructionPointFeatureParameters> resolve_construction_point_source(
    const DocumentState& document,
    const std::string& source_id);

// Walk `document.feature_history` in order and refresh every feature
// whose geometry is derived from upstream features:
//
//   * Face-based sketches (sketch.plane_id of the form
//     "<body_id>:face:<index>") get their `plane_frame` re-resolved
//     against the body geometry compiled from features earlier in the
//     history. If the upstream face is no longer present, the frame
//     stays at its last value and `dependency_broken` is set on the
//     feature so the UI can surface the warning.
//
//   * Extrudes copy their owning sketch's freshly-refreshed frame into
//     `extrude_parameters.plane_frame`, so the body compiler will
//     rebuild the extrude geometry on the new plane.
//
// The walk is single-pass: by the time we process feature K, every
// earlier sketch / extrude has already been refreshed, so face
// resolution at K sees the up-to-date upstream bodies. This naturally
// propagates N-level cascades (sketch -> extrude -> sketch on its
// face -> extrude -> ...).
//
// Mutators in `DocumentManager` should call this *after* applying their
// edit and *before* returning the document state, so consumers always
// see refreshed downstream features.
void refresh_history_dependencies(DocumentState& document);

}  // namespace polysmith::core
