// ── Sketch slot entity commands ────────────────────────────────
// Manage the parametric `slots` records. The boundary geometry is
// derived by refresh_sketch_slots inside refresh_sketch_derived_state,
// so each command just mutates the record and refreshes.

namespace {
void validate_slot_parameters(double length, double radius,
                              const std::string& mode) {
  if (radius <= 0.0) {
    throw std::runtime_error("Slot radius must be greater than zero");
  }
  if (length <= 0.0) {
    throw std::runtime_error("Slot length must be greater than zero");
  }
  // With length < 2·radius the two arc circles overlap and the boundary
  // self-intersects — the profile walk would misbehave.
  if (length < 2.0 * radius) {
    throw std::runtime_error(
        "Slot length must be at least twice the radius");
  }
  if (mode != "straight") {
    throw std::runtime_error("Unsupported slot mode: " + mode);
  }
}
}  // namespace

void add_sketch_slot(FeatureEntry& feature,
                     int slot_index,
                     double center_x,
                     double center_y,
                     double length,
                     double radius,
                     double rotation,
                     bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch slots");
  }
  validate_slot_parameters(length, radius, "straight");

  auto& parameters = *feature.sketch_parameters;
  // The center is a regular user-counter vertex (like circle centers)
  // so it is movable and dimension-able. The generated corner/arc-center
  // vertices carry the "vertex-slot-" prefix and are fixed by the
  // vertex rebuild.
  SketchSlot slot;
  slot.id = "slot-" + std::to_string(slot_index);
  slot.center_vertex_id =
      "vertex-" + std::to_string(parameters.next_vertex_index++);
  slot.center_x = center_x;
  slot.center_y = center_y;
  slot.length = length;
  slot.radius = radius;
  slot.rotation = rotation;
  slot.mode = "straight";
  slot.is_construction = is_construction;
  parameters.slots.push_back(std::move(slot));

  refresh_sketch_derived_state(feature);
}

void update_sketch_slot(FeatureEntry& feature,
                        const std::string& slot_id,
                        double center_x,
                        double center_y,
                        double length,
                        double radius,
                        double rotation) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch slots");
  }
  validate_slot_parameters(length, radius, "straight");

  auto& parameters = *feature.sketch_parameters;
  const auto slot_it = std::find_if(
      parameters.slots.begin(), parameters.slots.end(),
      [&](const SketchSlot& slot) { return slot.id == slot_id; });
  if (slot_it == parameters.slots.end()) {
    throw std::runtime_error("Sketch slot not found: " + slot_id);
  }

  slot_it->center_x = center_x;
  slot_it->center_y = center_y;
  slot_it->length = length;
  slot_it->radius = radius;
  slot_it->rotation = rotation;

  refresh_sketch_derived_state(feature);
}

void delete_sketch_slot(FeatureEntry& feature, const std::string& slot_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch slots");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto slot_it = std::find_if(
      parameters.slots.begin(), parameters.slots.end(),
      [&](const SketchSlot& slot) { return slot.id == slot_id; });
  if (slot_it == parameters.slots.end()) {
    throw std::runtime_error("Sketch slot not found: " + slot_id);
  }
  parameters.slots.erase(slot_it);

  // The next refresh strips the orphaned generated entities.
  refresh_sketch_derived_state(feature);
}
