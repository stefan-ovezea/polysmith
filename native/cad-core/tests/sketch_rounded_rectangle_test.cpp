// Rounded rectangle: a rectangle whose four corners are filleted alike, created
// as one undoable intent.
//
// Parity work (wiki/Sketch-Tools-Parity.md, T1.3). The value of building it from
// the ordinary rectangle + fillet paths rather than from four arcs is that the
// result keeps parametric fillet records: the radius stays editable and deleting
// a fillet restores its corner. These tests pin that.

#include <cmath>
#include <iostream>
#include <string>

#include "core/document/document.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

constexpr double kWidth = 80.0;
constexpr double kHeight = 60.0;
constexpr double kRadius = 8.0;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

const FeatureEntry* find_sketch(const DocumentState& document) {
  const FeatureEntry* found = nullptr;
  for (const auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) {
      found = &feature;
    }
  }
  return found;
}

DocumentState build(DocumentManager& manager) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  return manager.add_sketch_rounded_rectangle(0.0, 0.0, kWidth, kHeight,
                                              kRadius);
}

bool test_creates_four_fillets_and_a_closed_profile() {
  DocumentManager manager;
  const DocumentState document = build(manager);
  const FeatureEntry* sketch = find_sketch(document);
  if (!expect(sketch != nullptr, "rounded rect: sketch present")) {
    return false;
  }
  const auto& parameters = sketch->sketch_parameters.value();

  if (!expect(parameters.lines.size() == 4,
              "rounded rect: the four sides are still lines")) {
    return false;
  }
  if (!expect(parameters.arcs.size() == 4,
              "rounded rect: four corner arcs")) {
    return false;
  }
  if (!expect(parameters.fillets.size() == 4,
              "rounded rect: four parametric fillet records (regression: arcs "
              "welded on instead of fillets)")) {
    return false;
  }
  for (const auto& fillet : parameters.fillets) {
    if (!expect(std::abs(fillet.radius - kRadius) < 1e-9,
                "rounded rect: every fillet carries the requested radius")) {
      return false;
    }
  }
  // The outline is still one closed region, so it can be extruded.
  return expect(parameters.profiles.size() == 1,
                "rounded rect: the outline is a single closed profile");
}

bool test_radius_is_editable_and_validation_rejects_oversized() {
  DocumentManager manager;
  DocumentState document = build(manager);
  const FeatureEntry* sketch = find_sketch(document);
  if (!expect(sketch != nullptr &&
                  sketch->sketch_parameters->fillets.size() == 4,
              "rounded rect: four fillets to edit")) {
    return false;
  }
  const std::string fillet_id =
      sketch->sketch_parameters->fillets.front().id;

  document = manager.reenter_sketch(sketch->id);
  document = manager.update_sketch_fillet_radius(fillet_id, 5.0);
  sketch = find_sketch(document);
  if (!expect(sketch != nullptr,
              "rounded rect: sketch present after the radius edit")) {
    return false;
  }
  const auto& fillets = sketch->sketch_parameters->fillets;
  const bool any_five = std::any_of(
      fillets.begin(), fillets.end(),
      [](const auto& fillet) { return std::abs(fillet.radius - 5.0) < 1e-9; });
  if (!expect(any_five, "rounded rect: the edited radius reaches the record")) {
    return false;
  }

  // Half the shorter side is the limit; equal to it must be refused.
  bool rejected = false;
  try {
    manager.add_sketch_rounded_rectangle(0.0, 0.0, kWidth, kHeight,
                                         kHeight / 2.0);
  } catch (const std::exception&) {
    rejected = true;
  }
  return expect(rejected,
                "rounded rect: radius >= half the shorter side is refused");
}

bool test_it_is_one_undo_step() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rounded_rectangle(0.0, 0.0, kWidth, kHeight, kRadius);

  // Undo once: the rectangle AND its four fillets must go together.
  const DocumentState undone = manager.undo();
  const FeatureEntry* sketch = find_sketch(undone);
  if (!expect(sketch != nullptr, "rounded rect: sketch survives undo")) {
    return false;
  }
  const auto& parameters = sketch->sketch_parameters.value();
  if (!expect(parameters.lines.empty() && parameters.arcs.empty() &&
                  parameters.fillets.empty(),
              "rounded rect: one undo removes the sides, the arcs and the "
              "fillet records together (regression: five steps)")) {
    return false;
  }
  return true;
}

}  // namespace

int main() {
  if (!test_creates_four_fillets_and_a_closed_profile()) return 1;
  if (!test_radius_is_editable_and_validation_rejects_oversized()) return 1;
  if (!test_it_is_one_undo_step()) return 1;
  std::cout << "sketch_rounded_rectangle_test passed\n";
  return 0;
}
