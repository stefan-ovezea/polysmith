#pragma once

#include <TopoDS_Shape.hxx>

#include "core/feature.h"

namespace polysmith::core {

// Build a world-space OCCT solid for any single feature kind that
// produces geometry. Throws std::runtime_error for unsupported features.
TopoDS_Shape build_box_shape(const BoxFeatureParameters& parameters);
TopoDS_Shape build_cylinder_shape(const CylinderFeatureParameters& parameters);
TopoDS_Shape build_extrude_shape(const ExtrudeFeatureParameters& parameters);
TopoDS_Shape build_loft_shape(const LoftFeatureParameters& parameters);
TopoDS_Shape build_revolve_shape(const RevolveFeatureParameters& parameters);
TopoDS_Shape build_sweep_shape(const SweepFeatureParameters& parameters);
TopoDS_Shape build_hole_cutter_shape(const HoleFeatureParameters& parameters);
TopoDS_Shape build_fastener_shape(const FastenerFeatureParameters& parameters);

// Convenience that dispatches on FeatureEntry::kind. Returns a null shape
// for non-solid features (e.g. sketches) instead of throwing, so callers
// can simply skip nulls.
TopoDS_Shape build_feature_shape(const FeatureEntry& feature);

}  // namespace polysmith::core
