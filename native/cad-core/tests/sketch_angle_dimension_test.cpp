// Regression tests for the line-line angle dimension (kind "angle").
//
// Context: editing an angle dimension used to explode the sketch.  The UI
// sent the typed value in radians while the IPC boundary also converted
// degrees→radians, so 45° arrived as 0.79°; and a reflex target (>180°)
// was stored as-is even though planegcs L2LAngle enforces a directed
// angle in (-180°, 180°], so the first driving solve flipped the
// geometry by the reflex complement.  The manager API below is in core
// units (radians), so these tests cover the solver/storage side:
//   - a normal edit drives the second line about the shared pivot,
//   - a reflex value is normalised to the included angle and cannot move
//     the untouched line (the "explosion"),
//   - a T-junction measures the same dir_away quadrant the UI preview
//     shows (not the supplement),
//   - line_angle edits rotate the line about its start.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchDimension;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;

constexpr double kPi = 3.14159265358979323846;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

double degrees(double radians) { return radians * 180.0 / kPi; }

double line_angle_deg(const SketchLine& line) {
  return degrees(std::atan2(line.end_y - line.start_y,
                            line.end_x - line.start_x));
}

double line_length(const SketchLine& line) {
  return std::hypot(line.end_x - line.start_x, line.end_y - line.start_y);
}

const SketchLine& line_by_id(const SketchFeatureParameters& params,
                             const std::string& id) {
  for (const auto& line : params.lines) {
    if (line.id == id) return line;
  }
  return params.lines.front();
}

const SketchDimension* angle_dimension(const SketchFeatureParameters& params) {
  for (const auto& dimension : params.dimensions) {
    if (dimension.kind == "angle") return &dimension;
  }
  return nullptr;
}

// Returns a COPY of the id: pointers into a `sketch_params()` result die
// with the temporary, which is how the "dimension not found: <garbage>"
// aborts happen.
std::string angle_dimension_id(const SketchFeatureParameters& params) {
  const SketchDimension* dimension = angle_dimension(params);
  return dimension != nullptr ? dimension->id : std::string();
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) <= tolerance;
}

// Two lines sharing the origin at `a_deg` and `b_deg`.
DocumentState two_line_sketch(DocumentManager& manager, double a_deg,
                              double b_deg) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_line(
      0.0, 0.0, 80.0 * std::cos(a_deg * kPi / 180.0),
      80.0 * std::sin(a_deg * kPi / 180.0));
  return manager.add_sketch_line(
      0.0, 0.0, 50.0 * std::cos(b_deg * kPi / 180.0),
      50.0 * std::sin(b_deg * kPi / 180.0));
}

bool test_shared_endpoint_drives_included_angle() {
  DocumentManager manager;
  DocumentState document = two_line_sketch(manager, 20.0, 70.0);
  const std::string first = "line-1";
  const std::string second = "line-2";

  document = manager.add_sketch_angle_dimension(first, second);
  const std::string dimension_id = angle_dimension_id(sketch_params(document));
  auto params = sketch_params(document);
  const SketchDimension* added = angle_dimension(params);
  if (!expect(added != nullptr, "angle: dimension created")) return false;
  if (!expect(near(degrees(added->value), 50.0, 1e-6),
              "angle: initial value is the included 50 deg")) {
    return false;
  }
  if (!expect(added->is_auto, "angle: created as display-only (auto)")) {
    return false;
  }

  // Editing clears the auto flag and drives the second line about the
  // shared pivot; the first line stays put.
  document = manager.update_sketch_dimension(dimension_id,
                                             45.0 * kPi / 180.0);
  params = sketch_params(document);
  if (!expect(near(line_angle_deg(line_by_id(params, first)), 20.0, 1e-6),
              "angle: driving line A stays at 20 deg")) {
    return false;
  }
  if (!expect(near(line_angle_deg(line_by_id(params, second)), 65.0, 1e-6),
              "angle: line B solves to 20 + 45 = 65 deg")) {
    return false;
  }
  added = angle_dimension(params);
  if (!expect(added != nullptr && !added->is_auto && !added->driven,
              "angle: edited dimension is a driving manual dimension")) {
    return false;
  }
  if (!expect(near(degrees(added->value), 45.0, 1e-6),
              "angle: stored value is 45 deg")) {
    return false;
  }

  // Re-applying the same value is a no-op (no flip, no drift).
  document = manager.update_sketch_dimension(dimension_id,
                                             45.0 * kPi / 180.0);
  params = sketch_params(document);
  if (!expect(near(line_angle_deg(line_by_id(params, second)), 65.0, 1e-6),
              "angle: same-value edit keeps line B put")) {
    return false;
  }

  // Crossing 90° keeps the same branch and preserves both line lengths.
  document = manager.update_sketch_dimension(dimension_id,
                                             100.0 * kPi / 180.0);
  params = sketch_params(document);
  if (!expect(near(line_angle_deg(line_by_id(params, second)), 120.0, 1e-6),
              "angle: 100 deg drives line B to 120 deg")) {
    return false;
  }
  return expect(near(line_length(line_by_id(params, first)), 80.0, 1e-6) &&
                    near(line_length(line_by_id(params, second)), 50.0, 1e-6),
                "angle: driving preserves both line lengths");
}

bool test_reflex_target_is_normalised() {
  DocumentManager manager;
  DocumentState document = two_line_sketch(manager, 20.0, 70.0);
  const std::string first = "line-1";
  const std::string second = "line-2";
  document = manager.add_sketch_angle_dimension(first, second);
  const std::string dimension_id = angle_dimension_id(sketch_params(document));

  // A reflex target has no representation in the directed-angle solver.
  // It must normalise to the included 60° and rotate only line B by the
  // 10° delta — never flip the untouched line (pre-fix this left
  // line A at 37.5° and line B at -22.5°).
  document = manager.update_sketch_dimension(dimension_id,
                                             300.0 * kPi / 180.0);
  auto params = sketch_params(document);
  const SketchDimension* dimension = angle_dimension(params);
  if (!expect(dimension != nullptr, "reflex: dimension survives")) {
    return false;
  }
  if (!expect(!dimension->driven,
              "reflex: dimension stays driving (not rejected)")) {
    return false;
  }
  if (!expect(near(degrees(dimension->value), 60.0, 1e-6),
              "reflex: stored value normalises to 60 deg")) {
    return false;
  }
  if (!expect(near(line_angle_deg(line_by_id(params, first)), 20.0, 1e-6),
              "reflex: driving line A must not move")) {
    return false;
  }
  if (!expect(near(line_angle_deg(line_by_id(params, second)), 80.0, 1e-6),
              "reflex: line B rotates by the 10 deg included delta")) {
    return false;
  }

  // A reflex value at creation is clamped the same way (display-only).
  DocumentManager fresh;
  DocumentState created = two_line_sketch(fresh, 20.0, 70.0);
  created = fresh.add_sketch_angle_dimension("line-1", "line-2",
                                             300.0 * kPi / 180.0);
  const SketchDimension* created_dimension =
      angle_dimension(sketch_params(created));
  if (!expect(created_dimension != nullptr &&
                  near(degrees(created_dimension->value), 60.0, 1e-6),
              "reflex: creation clamps the supplied value to 60 deg")) {
    return false;
  }

  // A signed caller value (the UI's quadrant pick) keeps its direction:
  // the solver uses the sign to choose the matching ray, so a later
  // "Toggle Driving" does not flip the T-junction.
  DocumentManager signed_manager;
  DocumentState signed_doc = two_line_sketch(signed_manager, 20.0, 70.0);
  signed_doc = signed_manager.add_sketch_angle_dimension(
      "line-1", "line-2", -80.0 * kPi / 180.0);
  const SketchDimension* signed_dimension =
      angle_dimension(sketch_params(signed_doc));
  return expect(signed_dimension != nullptr &&
                    near(degrees(signed_dimension->value), -80.0, 1e-6),
                "signed: creation keeps the caller's directed sign");
}

bool test_t_connection_uses_dir_away_quadrant() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Host line at 10°; the incident line starts on the host at t = 0.6
  // (so the farther host endpoint is the start) and points at 110°.
  // dir_away on both lines gives |190 - 110| = 80°, while the old
  // smaller-angle heuristic gave the 100° supplement.
  const double host_angle = 10.0 * kPi / 180.0;
  const double hx = 100.0 * std::cos(host_angle);
  const double hy = 100.0 * std::sin(host_angle);
  DocumentState document = manager.add_sketch_line(0.0, 0.0, hx, hy);
  const double px = 0.6 * hx;
  const double py = 0.6 * hy;
  const double incident = 110.0 * kPi / 180.0;
  document = manager.add_sketch_line(
      px, py, px + 40.0 * std::cos(incident),
      py + 40.0 * std::sin(incident));

  // Pick the incident line first, like the dimension tool's first pick.
  document = manager.add_sketch_angle_dimension("line-2", "line-1");
  const std::string dimension_id = angle_dimension_id(sketch_params(document));
  auto params = sketch_params(document);
  const SketchDimension* added = angle_dimension(params);
  if (!expect(added != nullptr, "T: angle dimension created")) return false;
  if (!expect(near(degrees(std::abs(added->value)), 80.0, 1e-6),
              "T: created value is the dir_away 80 deg, not the 100 deg "
              "supplement")) {
    return false;
  }

  // Driving to 90° must rotate only the incident line (10° delta), not
  // the host.
  document = manager.update_sketch_dimension(dimension_id,
                                             90.0 * kPi / 180.0);
  params = sketch_params(document);
  if (!expect(near(line_angle_deg(line_by_id(params, "line-1")), 10.0, 1e-6),
              "T: host line stays at 10 deg")) {
    return false;
  }
  if (!expect(near(line_angle_deg(line_by_id(params, "line-2")), 100.0, 1e-6),
              "T: incident line drives to 100 deg")) {
    return false;
  }
  return expect(
      near(degrees(std::abs(angle_dimension(params)->value)), 90.0, 1e-6),
      "T: stored angle is 90 deg");
}

bool test_t_connection_toggle_driving_keeps_geometry() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Exactly 90° T-junction with the pivot PAST the host's midpoint:
  // dir_away points at the start while the solver's keep/swap rays tie
  // on |angle|.  Before the signed-target ray choice, toggling the
  // dimension to driving flipped both lines (10/100 -> 40.5/-49.5).
  const double host_angle = 10.0 * kPi / 180.0;
  const double hx = 100.0 * std::cos(host_angle);
  const double hy = 100.0 * std::sin(host_angle);
  DocumentState document = manager.add_sketch_line(0.0, 0.0, hx, hy);
  const double px = 0.6 * hx;
  const double py = 0.6 * hy;
  const double incident = 100.0 * kPi / 180.0;
  document = manager.add_sketch_line(
      px, py, px + 40.0 * std::cos(incident),
      py + 40.0 * std::sin(incident));

  document = manager.add_sketch_angle_dimension("line-2", "line-1");
  const std::string dimension_id = angle_dimension_id(sketch_params(document));
  if (!expect(!dimension_id.empty(), "toggle: angle dimension created")) {
    return false;
  }

  // First toggle makes it reference-only, second makes it driving
  // (clears is_auto) and runs the solver against the stored value.
  document = manager.toggle_sketch_dimension_driven(dimension_id);
  document = manager.toggle_sketch_dimension_driven(dimension_id);
  const auto params = sketch_params(document);
  if (!expect(near(line_angle_deg(line_by_id(params, "line-1")), 10.0, 1e-6),
              "toggle: host line stays at 10 deg")) {
    return false;
  }
  if (!expect(near(line_angle_deg(line_by_id(params, "line-2")), 100.0, 1e-6),
              "toggle: incident line stays at 100 deg (no 180 deg flip)")) {
    return false;
  }
  return expect(near(degrees(std::abs(angle_dimension(params)->value)), 90.0,
                     1e-6),
                "toggle: stored angle stays 90 deg");
}

bool test_line_angle_updates() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  const double angle = 20.0 * kPi / 180.0;
  DocumentState document = manager.add_sketch_line(
      0.0, 0.0, 80.0 * std::cos(angle), 80.0 * std::sin(angle));
  document = manager.add_sketch_line_angle_dimension("line-1");

  auto find_dim_id = [&](const DocumentState& doc) -> std::string {
    for (const auto& dimension : sketch_params(doc).dimensions) {
      if (dimension.kind == "line_angle" &&
          dimension.entity_id == "line-1") {
        return dimension.id;
      }
    }
    return std::string();
  };
  const std::string dimension_id = find_dim_id(document);
  if (!expect(!dimension_id.empty(), "line_angle: dimension created")) {
    return false;
  }

  document = manager.update_sketch_dimension(
      dimension_id, 45.0 * kPi / 180.0);
  auto params = sketch_params(document);
  if (!expect(near(line_angle_deg(line_by_id(params, "line-1")), 45.0, 1e-6),
              "line_angle: line drives to 45 deg")) {
    return false;
  }
  if (!expect(near(line_length(line_by_id(params, "line-1")), 80.0, 1e-6),
              "line_angle: length preserved")) {
    return false;
  }

  document = manager.update_sketch_dimension(
      dimension_id, 120.0 * kPi / 180.0);
  params = sketch_params(document);
  return expect(near(line_angle_deg(line_by_id(params, "line-1")), 120.0,
                     1e-6),
                "line_angle: line drives to 120 deg");
}

}  // namespace

int main() {
  if (!test_shared_endpoint_drives_included_angle()) return 1;
  if (!test_reflex_target_is_normalised()) return 1;
  if (!test_t_connection_uses_dir_away_quadrant()) return 1;
  if (!test_t_connection_toggle_driving_keeps_geometry()) return 1;
  if (!test_line_angle_updates()) return 1;
  std::cout << "sketch angle dimension tests passed\n";
  return 0;
}
