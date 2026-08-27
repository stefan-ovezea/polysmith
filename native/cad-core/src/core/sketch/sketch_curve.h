#pragma once

// Shared exact-curve model for the sketch system.
//
// Both consumers of sketch geometry — the profile walk
// (impl/sketch_profile_exact.inc) and the trim engine
// (trim_engine.{h,cpp}) — need the same parametric curve abstraction:
// points, tangents, sweep lifting, and pair intersections over
// line / circle / arc / ellipse / spline.  Keeping two private copies
// made them drift (trim never learned ellipses; the walk carried its
// own tolerances), which is how a trim could produce geometry the wire
// walk could not connect.
//
// This module is the single owner.  Split parameters are canonicalized
// exactly as the walk expects: lines/splines clamp to [0, 1], circular
// entities wrap angles into the sweep frame.  The analytic solvers for
// line/circle/arc pairs live in the trim engine; spline and ellipse
// pairs delegate to OCCT (spline_profile_occt.h).

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/sketch/sketch_profile_types.h"
#include "core/sketch/spline_profile_occt.h"

namespace polysmith::core {

constexpr double kExactPi = 3.14159265358979323846;
constexpr double kExactParamEps = 1e-9;
// Coincidence tolerance shared by the curve module and the profile
// walk — the same value the trim engine's kTrimCoincidentTolerance
// duplicates. Kept here so exact_curve_param_at_point can use it
// without depending on the walk's TU-local helpers.
constexpr double kProfileTolerance = 0.01;

// ── exact curve model ─────────────────────────────────────────────

struct ExactCurve {
  enum class Kind { kLine, kCircle, kArc, kEllipse, kSpline };
  Kind kind;
  std::string id;
  // line
  double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
  // circle / arc / ellipse
  double cx = 0.0, cy = 0.0, r = 0.0;
  // ellipse extras
  double b = 0.0;              // minor radius
  double rotation = 0.0;       // major-axis angle
  bool ccw = true;             // stored sweep direction (circles: ccw)
  double sweep_start = 0.0;    // wrapped angle of the entity's start param
  double sweep_end = 0.0;      // wrapped angle of the entity's end param (arcs)
  // spline extras — param in [0, 1]
  std::vector<double> pole_xs;
  std::vector<double> pole_ys;
  int spline_degree = 3;
  // shared vertex ids for endpoint pre-union (lines, arcs, splines)
  std::string start_vertex_id;
  std::string end_vertex_id;
};

double exact_wrap_angle(double a);

SketchProfilePoint exact_curve_point(const ExactCurve& c, double param);

// Unit tangent of INCREASING parameter at `param` (line: start->end;
// circle: ccw; arc: along the stored sweep).
void exact_curve_tangent(const ExactCurve& c, double param, double& tx,
                         double& ty);

// Lifts a wrapped angle into the sweep frame of a circular entity so
// params can be ordered along the sweep direction.
double exact_lift_to_sweep(const ExactCurve& c, double wrapped);

bool exact_angle_in_sweep(double wrapped, const ExactCurve& arc);

// Param on a line / circle / arc / spline from a POINT on it — the
// inverse of exact_curve_point. Returns nullopt when the point is not
// within profile tolerance of the curve.
std::optional<double> exact_curve_param_at_point(const ExactCurve& c,
                                                 double x, double y);

// Intersections between a spline (first arg) and any other curve kind
// (second arg) via OCCT.  Fills `out` with (spline param, other param)
// pairs; the other param is re-derived from the intersection point so
// only the spline's parameterization comes from OCCT.
void exact_spline_intersections(const ExactCurve& spline,
                                const ExactCurve& other,
                                std::vector<std::pair<double, double>>& out);

// All exact intersections between two curves: fills `out` with
// (param on A, param on B) records.  Covers every kind pairing;
// line/circle/arc pairs use the trim engine's analytic solvers,
// spline/ellipse pairs delegate to OCCT.
void sketch_curve_intersections(const ExactCurve& A, const ExactCurve& B,
                                std::vector<std::pair<double, double>>& out);

}  // namespace polysmith::core
