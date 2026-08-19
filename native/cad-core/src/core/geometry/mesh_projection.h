#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include "core/document/plane_frame.h"

namespace polysmith::core {

struct SketchFeatureParameters;

// Cross-section polylines (world space): the intersection of the mesh
// with the plane defined by `frame`. Coplanar faces contribute their
// boundary edges so a body sitting flat on the sketch plane still
// projects its outline. Curved mesh edges come back as polylines —
// STL facets carry no curvature.
std::vector<std::vector<gp_Pnt>> compute_mesh_section_polylines(
    const TopoDS_Shape& mesh, const PlaneFrame& frame);

// Silhouette polylines (world space, lying in the frame plane): the
// visible outline of the mesh as seen along the frame normal (Fusion
// 360 "Project" semantics — outline only, not interior facet edges).
std::vector<std::vector<gp_Pnt>> compute_mesh_silhouette_polylines(
    const TopoDS_Shape& mesh, const PlaneFrame& frame);

// Projection frame for a sketch: the stored plane_frame when present,
// else the origin ref-plane frame (origin ref-plane sketches carry no
// frame — the legacy axis mapping in the same convention).
std::optional<PlaneFrame> resolve_sketch_projection_frame(
    const SketchFeatureParameters& sketch);

// Flatten a world point into sketch-local (x, y) coordinates.
std::pair<double, double> world_to_sketch_local(const PlaneFrame& frame,
                                                const gp_Pnt& point);

}  // namespace polysmith::core
