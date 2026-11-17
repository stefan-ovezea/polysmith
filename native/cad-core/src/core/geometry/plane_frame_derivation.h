#pragma once

#include <optional>

#include <TopoDS_Face.hxx>

#include "core/document/plane_frame.h"

namespace polysmith::core {

// Canonical sketch frame for a planar face.
//
// The kernel's own plane placement (`gp_Pln::Position()`) is not a
// property of the geometry: it is whatever the modeling system that
// produced the face happened to write down. Faces built by PolySmith
// (box, extrude, revolve, ...) carry world-aligned axes, so reading the
// placement verbatim looked correct — but a face that arrives through
// STEP/IGES import inherits the exporter's plane rotation. A plate
// lying flat in XY can come back with its plane rotated 180 degrees
// about Z, and the sketch derived from it then pointed +X along world
// -X and +Y along world -Y.
//
// The roll is therefore derived from the face normal alone, so the same
// physical plane always yields the same sketch axes regardless of which
// file it came from:
//
//   * non-horizontal face: sketch +Y is world +Z projected into the
//     plane — the model's up direction stays up in the sketch.
//   * horizontal face (normal within ~1e-9 of +-Z): the projection above
//     is degenerate, so world +Y supplies the roll instead (the familiar
//     top view of a Z-up model).
//   * sketch +X = +Y x +normal, which keeps the basis right-handed. Every
//     face-derived frame already used this convention, so the wire winding
//     and hole orientation that depend on cross(x_axis, y_axis) do not
//     change.
//
// The origin stays at the surface's location (a point on the face's
// plane), so a sketch keeps the reference point its face provides and
// existing face-based sketches do not move.
//
// Returns nullopt when the face is null, not planar, degenerate, or when
// the OCCT call throws.
std::optional<PlaneFrame> canonical_planar_face_frame(const TopoDS_Face& face);

}  // namespace polysmith::core
