#pragma once

#include "core/feature.h"

namespace polysmith::core {

struct ConstructionAxisFrame {
  double start_x;
  double start_y;
  double start_z;
  double end_x;
  double end_y;
  double end_z;
};

// Build a fresh "construction_plane" FeatureEntry from a
// caller-supplied source plane frame and an offset along the source's
// normal. The caller (DocumentManager) is responsible for resolving
// the source's frame from the document state — we deliberately keep
// this module free of upstream-resolution logic so it can be reused
// from any callsite that already has a frame in hand.
FeatureEntry create_construction_plane_feature(
    int feature_index,
    const std::string& source_plane_id,
    double offset,
    const PlaneFrame& source_frame);

FeatureEntry create_midplane_feature(int feature_index,
                                     const std::string& first_source_id,
                                     const std::string& second_source_id,
                                     const PlaneFrame& first_frame,
                                     const PlaneFrame& second_frame);

FeatureEntry create_tangent_plane_feature(int feature_index,
                                          const std::string& source_face_id,
                                          const PlaneFrame& tangent_frame);

FeatureEntry create_angle_plane_feature(int feature_index,
                                        const std::string& source_plane_id,
                                        const std::string& source_axis_id,
                                        double angle_degrees,
                                        const PlaneFrame& source_frame,
                                        const ConstructionAxisFrame& axis);

// Update an existing construction_plane feature's offset and
// re-derive its cached frame from `source_frame`. Throws when the
// feature is not a construction_plane.
void update_construction_plane(FeatureEntry& feature,
                               double offset,
                               const PlaneFrame& source_frame);
void update_angle_plane(FeatureEntry& feature,
                        double angle_degrees,
                        const PlaneFrame& source_frame,
                        const ConstructionAxisFrame& axis);

// Helper exposed for the dependency walker. Slides `source_frame`
// along its own normal by `offset`, leaving the basis vectors
// intact. The returned frame is what the construction-plane feature
// stores in its parameters.
PlaneFrame derive_offset_frame(const PlaneFrame& source_frame, double offset);
PlaneFrame derive_midplane_frame(const PlaneFrame& first_frame,
                                 const PlaneFrame& second_frame);
PlaneFrame derive_angle_plane_frame(const PlaneFrame& source_frame,
                                    const ConstructionAxisFrame& axis,
                                    double angle_degrees);

}  // namespace polysmith::core
