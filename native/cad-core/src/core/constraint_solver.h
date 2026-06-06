#pragma once

#include "core/feature.h"
#include <GCS.h>
#include <Geo.h>
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace polysmith::core {

/// Bridges Polysmith's sketch constraint data model to the planegcs
/// (FreeCAD PlaneGCS) 2D geometric constraint solver.
///
/// Usage:
///   ConstraintSolver solver;
///   solver.build(params);
///   auto result = solver.solve();
///   solver.apply(params);
class ConstraintSolver {
public:
    struct SolveResult {
        GCS::SolveStatus status = GCS::Failed;
        int dofs = -1;
        std::vector<int> conflicting;
        std::vector<int> redundant;
        std::vector<int> partially_redundant;

        bool ok() const {
            return status == GCS::Success || status == GCS::Converged;
        }
    };

    ConstraintSolver() = default;
    ~ConstraintSolver() = default;

    /// Build the planegcs system from sketch parameters. Must be called
    /// before solve(). Rebuilds the entire internal state — call this
    /// every time the sketch changes.
    void build(const SketchFeatureParameters& params);

    /// Run the solver. Returns status + diagnostics.
    SolveResult solve(GCS::Algorithm alg = GCS::DogLeg);

    /// Write the solved point coordinates back into the sketch parameters.
    /// Call after a successful solve().
    void apply(SketchFeatureParameters& params) const;

    /// Number of degrees of freedom after the last solve/diagnose.
    int dofs() const;

private:
    // Per-point parameter indices into param_storage_
    struct PointMapping {
        int x_idx;
        int y_idx;
        bool is_fixed;
    };

    // Owns all double values (point coordinates, dimensional values).
    // Must not reallocate after GCS::Point objects are created pointing
    // into it — use reserve() to pre-allocate.
    std::vector<double> param_storage_;

    // GCS geometry objects, referencing into param_storage_.
    std::vector<GCS::Point> gcs_points_;
    std::vector<GCS::Line> gcs_lines_;
    std::vector<GCS::Circle> gcs_circles_;

    // Maps sketch entity IDs to GCS geometry indices.
    std::map<std::string, size_t> point_id_to_index_;
    std::map<std::string, size_t> line_id_to_index_;
    std::map<std::string, size_t> circle_id_to_index_;

    // The solver system — owned, rebuilt on each build() call.
    std::unique_ptr<GCS::System> system_;

    // Results from last solve.
    SolveResult last_result_;
};

} // namespace polysmith::core
