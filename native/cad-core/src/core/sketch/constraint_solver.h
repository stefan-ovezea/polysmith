#pragma once

#include "core/sketch/sketch_types.h"
#include <GCS.h>
#include <Geo.h>
#include <deque>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>

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
    // `diagnose` runs planegcs' redundancy analysis, which is by far the most
    // expensive part of a solve (it re-solves subsystems per candidate). It is
    // only needed when the caller wants conflict/redundant counts or a DOF
    // number; a plain geometry enforcement pass does not.
    SolveResult solve(GCS::Algorithm alg = GCS::DogLeg,
                      bool diagnose = true);

    /// Write the solved point coordinates back into the sketch parameters.
    /// Call after a successful solve().
    void apply(SketchFeatureParameters& params) const;

    /// Number of degrees of freedom after the last solve/diagnose.
    int dofs() const;

    /// D-008 diagnostic: a human-readable line per `point_on_circle` constraint
    /// describing what the solver did to it — whether the vertex was declared an
    /// unknown (its proxy for "is it in planegcs' plist"), the vertex position in
    /// solver space (`param_storage_`) vs the `params` record, and the radius the
    /// solver drove vs the radius slot `apply()` reads back. `stage` names the
    /// call ("post-solve" / "post-apply"). Returns an empty string when the sketch
    /// has no `point_on_circle` constraints. Always computed; callers gate logging
    /// behind PS_TRACE_POINT_ON_CIRCLE so the app does not log by default.
    std::string point_on_circle_trace(const SketchFeatureParameters& params,
                                      const char* stage) const;

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
    // A DEQUE, not a vector, ON PURPOSE: every element address in here is handed to
    // planegcs as a solver parameter (`gc.rad = &param_storage_[i]`, dimension values,
    // radius pins), so a reallocation would leave every previously stored pointer
    // dangling and the constraints reading garbage - constraints that then silently do
    // nothing, which is exactly how D-008 looked (a point_on_circle that was "stored
    // correctly", reported solver_ok, and never moved the point). deque keeps element
    // addresses stable across push_back.
    std::deque<double> param_storage_;

    // GCS geometry objects, referencing into param_storage_.
    std::vector<GCS::Point> gcs_points_;
    std::vector<GCS::Line> gcs_lines_;
    std::vector<GCS::Circle> gcs_circles_;
    std::vector<GCS::Arc> gcs_arcs_;

    // Parameter indices for each arc's rad / startAngle / endAngle,
    // aligned with gcs_arcs_.
    struct ArcParamMapping {
        size_t rad_idx;
        size_t start_angle_idx;
        size_t end_angle_idx;
    };
    std::vector<ArcParamMapping> arc_param_idx_;

    // Maps sketch entity IDs to GCS geometry indices.
    std::map<std::string, size_t> point_id_to_index_;
    std::map<std::string, size_t> line_id_to_index_;
    std::map<std::string, size_t> circle_id_to_index_;
    std::map<std::string, size_t> arc_id_to_index_;

    // Vertex ids whose x/y were pushed to `unknown_params` during build() — i.e.
    // the vertices this bridge declared as solver unknowns (planegcs' plist is
    // private, so this records what *we* declared). Used by point_on_circle_trace.
    std::set<std::string> declared_unknown_ids_;

    // The solver system — owned, rebuilt on each build() call.
    std::unique_ptr<GCS::System> system_;

    // Results from last solve.
    SolveResult last_result_;
};

} // namespace polysmith::core
