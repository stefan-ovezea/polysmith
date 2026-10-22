#pragma once

#include <optional>
#include <string>
#include <utility>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/document/body_feature_types.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

// ── Extrude extent resolution ───────────────────────────────────────────
gp_Vec extrude_normal_vector(const ExtrudeFeatureParameters& params,
                             double side_sign);
gp_Pnt extrude_plane_origin(const ExtrudeFeatureParameters& params);
std::optional<std::pair<double, double>> body_projection_range(
    const TopoDS_Shape& shape, const gp_Pnt& origin, const gp_Vec& direction);
std::optional<double> distance_to_body_boundary(
    const CompiledBodies& compiled, const std::optional<std::string>& body_id,
    const gp_Pnt& origin, const gp_Vec& direction, double start_offset,
    bool through_all);
std::optional<double> distance_to_face_plane(const DocumentState& document,
                                             const std::string& face_id,
                                             const gp_Pnt& origin,
                                             const gp_Vec& direction,
                                             double start_offset);
void resolve_extrude_side_extent(const DocumentState& document,
                                 ExtrudeFeatureParameters& params,
                                 ExtrudeFeatureParameters::SideParameters& side,
                                 double side_sign);
void normalize_extrude_parameters(const DocumentState& document,
                                  ExtrudeFeatureParameters& params);

// ── Extrude body-contact detection ──────────────────────────────────────
bool shapes_intersect_with_volume(const TopoDS_Shape& a, const TopoDS_Shape& b);
bool shapes_touch_without_volume(const TopoDS_Shape& a, const TopoDS_Shape& b);
std::optional<std::string> find_intersecting_body_for_extrude(
    const DocumentState& document, const ExtrudeFeatureParameters& parameters);
std::optional<std::string> find_touching_body_for_extrude(
    const DocumentState& document, const ExtrudeFeatureParameters& parameters);

}  // namespace polysmith::core
