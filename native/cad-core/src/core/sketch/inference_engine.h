#pragma once

#include "core/feature.h"

namespace polysmith::core {

// Run inference on a newly committed sketch entity. Checks for
// coincident endpoints, concentric centers, and other auto-detectable
// geometric relationships. Populates the constraints[] vector with
// SketchConstraint entries so the constraint graph stays consistent.
//
// Must be called AFTER the entity is fully added to the sketch but
// BEFORE refresh_sketch_derived_state.
//
// Returns the number of constraints that were inferred (0 if none).
int run_inference_on_new_line(SketchFeatureParameters& params,
                              SketchLine& line);

int run_inference_on_new_circle(SketchFeatureParameters& params,
                                SketchCircle& circle);

} // namespace polysmith::core
