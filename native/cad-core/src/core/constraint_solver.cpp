#include "core/constraint_solver.h"
#include <GCS.h>
#include <Geo.h>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace polysmith::core {

void ConstraintSolver::build(const SketchFeatureParameters& params) {
    // --- Phase 1: count and allocate parameter storage ---
    // Pre-count to avoid reallocation (which would invalidate double*
    // pointers stored in GCS::Point objects).
    // We read geometry from lines/circles (canonical storage), not from
    // params.points[] (which is a deduplicated view rebuilt later by
    // rebuild_sketch_points). This ensures the solver sees the current
    // geometry even when called between rebuilds (e.g. during drag).
    const size_t num_points = params.points.size();
    const size_t num_circles = params.circles.size();
    // Count driving dimensions for parameter reservation.
    // Manual dims always drive; auto dims drive only with expression.
    size_t driving_dim_count = 0;
    for (const auto& dim : params.dimensions) {
        if (dim.driven) continue;
        if (dim.is_auto && dim.expression.empty()) continue;
        ++driving_dim_count;
    }
    const size_t num_params =
        num_points * 2 + num_circles * 3 + driving_dim_count;

    param_storage_.clear();
    param_storage_.reserve(num_params);

    gcs_points_.clear();
    gcs_points_.reserve(num_points);
    point_id_to_index_.clear();

    // Fill parameter storage. For each sketch point, read the current
    // coordinate from the owning line/circle if possible; fall back to
    // the point's stored value.
    for (size_t i = 0; i < num_points; ++i) {
        const auto& sp = params.points[i];
        double px = sp.x;
        double py = sp.y;

        // If this point is a line endpoint or circle center, read the
        // current coordinate from that entity — the point vector may
        // be stale between rebuild_sketch_points calls.
        if (sp.kind == "endpoint") {
            for (const auto& line : params.lines) {
                if (line.start_point_id == sp.id) {
                    px = line.start_x; py = line.start_y; break;
                }
                if (line.end_point_id == sp.id) {
                    px = line.end_x; py = line.end_y; break;
                }
            }
        } else if (sp.kind == "center") {
            for (const auto& circle : params.circles) {
                std::string center_id = "point-circle-" + circle.id + "-center";
                if (sp.id == center_id) {
                    px = circle.center_x; py = circle.center_y; break;
                }
            }
        }

        param_storage_.push_back(px);
        param_storage_.push_back(py);

        GCS::Point gp;
        gp.x = &param_storage_[i * 2];
        gp.y = &param_storage_[i * 2 + 1];
        gcs_points_.push_back(gp);
        point_id_to_index_[sp.id] = i;
    }

    // Map line entities.
    gcs_lines_.clear();
    gcs_lines_.reserve(params.lines.size());
    line_id_to_index_.clear();
    for (size_t i = 0; i < params.lines.size(); ++i) {
        const auto& sl = params.lines[i];
        auto start_it = point_id_to_index_.find(sl.start_point_id);
        auto end_it = point_id_to_index_.find(sl.end_point_id);
        if (start_it == point_id_to_index_.end() ||
            end_it == point_id_to_index_.end()) {
            // Missing point reference — skip this line.
            continue;
        }
        GCS::Line gl;
        gl.p1 = gcs_points_[start_it->second];
        gl.p2 = gcs_points_[end_it->second];
        gcs_lines_.push_back(gl);
        line_id_to_index_[sl.id] = gcs_lines_.size() - 1;
    }

    // Map circle entities.
    gcs_circles_.clear();
    gcs_circles_.reserve(params.circles.size());
    circle_id_to_index_.clear();
    for (size_t i = 0; i < params.circles.size(); ++i) {
        const auto& sc = params.circles[i];
        // Compute the radius from the stored value.
        // The circle's center is a sketch point referenced by the circle.
        // For now, use a dedicated parameter for the radius.
        param_storage_.push_back(sc.radius);

        GCS::Circle gc;
        // Find the center point — circles have a dedicated center point.
        // The center point id is "point-circle-{circle_id}-center"
        std::string center_point_id =
            "point-circle-" + sc.id + "-center";
        auto center_it = point_id_to_index_.find(center_point_id);
        if (center_it != point_id_to_index_.end()) {
            gc.center = gcs_points_[center_it->second];
        } else {
            // Fallback: use the circle's stored center coordinates
            // by creating an anonymous point in param_storage_.
            // This shouldn't normally happen.
            param_storage_.push_back(sc.center_x);
            param_storage_.push_back(sc.center_y);
            GCS::Point anon_center;
            anon_center.x = &param_storage_[param_storage_.size() - 2];
            anon_center.y = &param_storage_[param_storage_.size() - 1];
            gc.center = anon_center;
        }
        gc.rad = &param_storage_[param_storage_.size() - 3]; // radius param
        gcs_circles_.push_back(gc);
        circle_id_to_index_[sc.id] = gcs_circles_.size() - 1;
    }

    // --- Phase 2: build the GCS::System ---
    system_ = std::make_unique<GCS::System>();
    system_->clear();

    // Declare parameters. Fixed points are NOT added to unknowns.
    // Circle radii are unknowns so the solver can adjust them to
    // satisfy radius constraints.
    GCS::VEC_pD unknown_params;
    for (size_t i = 0; i < num_points; ++i) {
        if (!params.points[i].is_fixed) {
            unknown_params.push_back(&param_storage_[i * 2]);     // x
            unknown_params.push_back(&param_storage_[i * 2 + 1]); // y
        }
    }
    // Circle radius parameters start after the point params.
    size_t radius_base = num_points * 2;
    for (size_t i = 0; i < num_circles; ++i) {
        unknown_params.push_back(&param_storage_[radius_base + i]);
    }
    system_->declareUnknowns(unknown_params);

    // --- Phase 3: add constraints ---

    // SketchLine inline constraints (horizontal / vertical).
    for (const auto& sl : params.lines) {
        auto it = line_id_to_index_.find(sl.id);
        if (it == line_id_to_index_.end()) continue;
        auto& gl = gcs_lines_[it->second];
        if (sl.constraint.has_value()) {
            if (sl.constraint.value() == "horizontal") {
                system_->addConstraintHorizontal(gl);
            } else if (sl.constraint.value() == "vertical") {
                system_->addConstraintVertical(gl);
            }
        }
    }

    // SketchLineRelation constraints (parallel, perpendicular, equal).
    for (const auto& rel : params.line_relations) {
        auto it1 = line_id_to_index_.find(rel.first_line_id);
        auto it2 = line_id_to_index_.find(rel.second_line_id);
        if (it1 == line_id_to_index_.end() ||
            it2 == line_id_to_index_.end()) continue;
        auto& gl1 = gcs_lines_[it1->second];
        auto& gl2 = gcs_lines_[it2->second];
        if (rel.kind == "parallel") {
            system_->addConstraintParallel(gl1, gl2);
        } else if (rel.kind == "perpendicular") {
            system_->addConstraintPerpendicular(gl1, gl2);
        } else if (rel.kind == "equal_length") {
            system_->addConstraintEqualLength(gl1, gl2);
        } else if (rel.kind == "tangent") {
            // Tangent line-line? Not directly supported by planegcs.
            // For line-circle tangent, we need the circle reference.
            // Skip for now; handled in a later phase.
        }
    }

    // SketchConstraint entries (coincident, concentric).
    for (const auto& sc : params.constraints) {
        if (sc.kind == "coincident" && sc.target_ids.size() >= 2) {
            auto it1 = point_id_to_index_.find(sc.target_ids[0]);
            auto it2 = point_id_to_index_.find(sc.target_ids[1]);
            if (it1 != point_id_to_index_.end() &&
                it2 != point_id_to_index_.end()) {
                system_->addConstraintP2PCoincident(
                    gcs_points_[it1->second],
                    gcs_points_[it2->second]);
            }
        } else if (sc.kind == "concentric" && sc.target_ids.size() >= 2) {
            // Concentric circles — add coincident on their center points.
            // The target_ids reference circle ids, not point ids.
            // For now, this is handled via point coincidence on circle
            // center points (inference engine creates these).
            // TODO: proper circle concentric constraint.
            auto cit1 = circle_id_to_index_.find(sc.target_ids[0]);
            auto cit2 = circle_id_to_index_.find(sc.target_ids[1]);
            if (cit1 != circle_id_to_index_.end() &&
                cit2 != circle_id_to_index_.end()) {
                // Use the equal constraint on center coordinates
                // (this is approximate — a proper concentric
                //  constraint would be better)
            }
        }
    }

    // Midpoint and point-line anchors: constrain the anchored point
    // to lie on the host line. Full parametric position enforcement
    // (t=0.5 for midpoints, t=t for point-line) is a follow-up.
    for (const auto& anchor : params.midpoint_anchors) {
        auto pit = point_id_to_index_.find(anchor.point_id);
        auto lit = line_id_to_index_.find(anchor.line_id);
        if (pit != point_id_to_index_.end() &&
            lit != line_id_to_index_.end()) {
            system_->addConstraintPointOnLine(
                gcs_points_[pit->second],
                gcs_lines_[lit->second]);
        }
    }

    for (const auto& anchor : params.point_line_anchors) {
        auto pit = point_id_to_index_.find(anchor.point_id);
        auto lit = line_id_to_index_.find(anchor.line_id);
        if (pit != point_id_to_index_.end() &&
            lit != line_id_to_index_.end()) {
            system_->addConstraintPointOnLine(
                gcs_points_[pit->second],
                gcs_lines_[lit->second]);
        }
    }

    // --- Phase 4: dimensional constraints (from dimensions) ---
    // Manual dimensions (is_auto=false) are always driving constraints
    // at their current measured value. Auto-dimensions (is_auto=true)
    // become driving constraints only when the user types a value
    // (non-empty expression).
    for (const auto& dim : params.dimensions) {
        if (dim.driven) continue;  // reference-only, not a constraint
        // Auto-dimensions without a user-set expression are display-only.
        if (dim.is_auto && dim.expression.empty()) continue;

        // Allocate a parameter for the constraint target value.
        size_t val_idx = param_storage_.size();
        param_storage_.push_back(dim.value);
        double* val_ptr = &param_storage_[val_idx];

        if (dim.kind == "line_length") {
            auto lit = line_id_to_index_.find(dim.entity_id);
            if (lit != line_id_to_index_.end()) {
                auto& gl = gcs_lines_[lit->second];
                system_->addConstraintP2PDistance(gl.p1, gl.p2, val_ptr);
            }
        } else if (dim.kind == "line_angle") {
            auto lit = line_id_to_index_.find(dim.entity_id);
            if (lit != line_id_to_index_.end()) {
                auto& gl = gcs_lines_[lit->second];
                // P2PAngle constrains the angle of the vector from p1 to p2.
                system_->addConstraintP2PAngle(gl.p1, gl.p2, val_ptr, 0.0);
            }
        } else if (dim.kind == "circle_radius") {
            auto cit = circle_id_to_index_.find(dim.entity_id);
            if (cit != circle_id_to_index_.end()) {
                system_->addConstraintCircleRadius(
                    gcs_circles_[cit->second], val_ptr);
            }
        } else if (dim.kind == "angle" && !dim.secondary_entity_id.empty()) {
            // Angle between two lines.
            auto lit1 = line_id_to_index_.find(dim.entity_id);
            auto lit2 = line_id_to_index_.find(dim.secondary_entity_id);
            if (lit1 != line_id_to_index_.end() &&
                lit2 != line_id_to_index_.end()) {
                system_->addConstraintL2LAngle(
                    gcs_lines_[lit1->second],
                    gcs_lines_[lit2->second],
                    val_ptr);
            }
        } else if (dim.kind == "point_distance" &&
                   !dim.secondary_entity_id.empty()) {
            auto pit1 = point_id_to_index_.find(dim.entity_id);
            auto pit2 = point_id_to_index_.find(dim.secondary_entity_id);
            if (pit1 != point_id_to_index_.end() &&
                pit2 != point_id_to_index_.end()) {
                system_->addConstraintP2PDistance(
                    gcs_points_[pit1->second],
                    gcs_points_[pit2->second],
                    val_ptr);
            }
        }
    }
}

ConstraintSolver::SolveResult ConstraintSolver::solve(GCS::Algorithm alg) {
    SolveResult result;
    if (!system_) {
        result.status = GCS::Failed;
        return result;
    }

    system_->initSolution(alg);
    int status = system_->solve(/*isFine=*/true, alg);
    result.status = static_cast<GCS::SolveStatus>(status);

    // Run diagnose to get DOF and conflicting/redundant info.
    system_->diagnose(alg);
    result.dofs = system_->dofsNumber();

    GCS::VEC_I conflicting, redundant, partially_redundant;
    system_->getConflicting(conflicting);
    system_->getRedundant(redundant);
    system_->getPartiallyRedundant(partially_redundant);
    result.conflicting.assign(conflicting.begin(), conflicting.end());
    result.redundant.assign(redundant.begin(), redundant.end());
    result.partially_redundant.assign(
        partially_redundant.begin(), partially_redundant.end());

    if (result.ok()) {
        system_->applySolution();
    }

    last_result_ = result;
    return result;
}

void ConstraintSolver::apply(SketchFeatureParameters& params) const {
    if (!last_result_.ok()) return;

    // Write solved coordinates back to the canonical storage (lines
    // and circles). Do NOT write to params.points[] — that array is
    // rebuilt from lines/circles by rebuild_sketch_points() later in
    // refresh_sketch_derived_state.

    // Update line endpoints from their points.
    for (auto& sl : params.lines) {
        auto sit = point_id_to_index_.find(sl.start_point_id);
        auto eit = point_id_to_index_.find(sl.end_point_id);
        if (sit != point_id_to_index_.end()) {
            sl.start_x = param_storage_[sit->second * 2];
            sl.start_y = param_storage_[sit->second * 2 + 1];
        }
        if (eit != point_id_to_index_.end()) {
            sl.end_x = param_storage_[eit->second * 2];
            sl.end_y = param_storage_[eit->second * 2 + 1];
        }
    }

    // Update circle center positions and radii.
    size_t radius_base = params.points.size() * 2;
    for (size_t i = 0; i < params.circles.size(); ++i) {
        auto& sc = params.circles[i];
        std::string center_point_id =
            "point-circle-" + sc.id + "-center";
        auto cit = point_id_to_index_.find(center_point_id);
        if (cit != point_id_to_index_.end()) {
            sc.center_x = param_storage_[cit->second * 2];
            sc.center_y = param_storage_[cit->second * 2 + 1];
        }
        sc.radius = param_storage_[radius_base + i];
    }
}

int ConstraintSolver::dofs() const {
    return last_result_.dofs;
}

} // namespace polysmith::core
