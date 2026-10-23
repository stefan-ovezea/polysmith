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

// Arc-slot rules (T2.3): the centreline radius must leave a positive inner
// boundary, and the sweep must be a partial turn — a full 2π sweep puts the
// two radial end caps on top of each other and the loop stops being simple.
void validate_arc_slot_parameters(double arc_radius, double radius,
                                  double sweep_angle) {
  if (radius <= 0.0) {
    throw std::runtime_error("Slot radius must be greater than zero");
  }
  if (arc_radius <= radius) {
    throw std::runtime_error(
        "Arc slot radius must be greater than half the slot width");
  }
  if (sweep_angle <= 0.0) {
    throw std::runtime_error("Arc slot sweep must be greater than zero");
  }
  constexpr double kTwoPi = 6.283185307179586;
  if (sweep_angle >= kTwoPi - 1.0e-9) {
    throw std::runtime_error("Arc slot sweep must be less than a full turn");
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
                     bool is_construction,
                     const std::string& mode,
                     double arc_radius,
                     double start_angle,
                     double sweep_angle) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch slots");
  }
  const bool arc_mode = mode == "arc";
  if (arc_mode) {
    validate_arc_slot_parameters(arc_radius, radius, sweep_angle);
  } else if (mode.empty() || mode == "straight") {
    validate_slot_parameters(length, radius, "straight");
  } else {
    throw std::runtime_error("Unsupported slot mode: " + mode);
  }

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
  slot.radius = radius;
  if (arc_mode) {
    // Arc mode reuses center_x/center_y as the ARC CENTRE, so the movable
    // centre vertex moves the whole slot.
    slot.mode = "arc";
    slot.arc_radius = arc_radius;
    slot.start_angle = start_angle;
    slot.sweep_angle = sweep_angle;
  } else {
    slot.mode = "straight";
    slot.length = length;
    slot.rotation = rotation;
  }
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
                        double rotation,
                        double arc_radius,
                        double start_angle,
                        double sweep_angle) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch slots");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto slot_it = std::find_if(
      parameters.slots.begin(), parameters.slots.end(),
      [&](const SketchSlot& slot) { return slot.id == slot_id; });
  if (slot_it == parameters.slots.end()) {
    throw std::runtime_error("Sketch slot not found: " + slot_id);
  }

  // The edit form follows the slot's own mode: an arc slot has no length or
  // rotation, a straight slot has no centreline radius or sweep.
  if (slot_it->mode == "arc") {
    // Absent arc parameters keep the stored ones, so a plain centre/width
    // edit cannot silently collapse the arc.
    const double next_arc_radius = arc_radius > 0.0 ? arc_radius : slot_it->arc_radius;
    const double next_sweep = sweep_angle > 0.0 ? sweep_angle : slot_it->sweep_angle;
    validate_arc_slot_parameters(next_arc_radius, radius, next_sweep);
    slot_it->center_x = center_x;
    slot_it->center_y = center_y;
    slot_it->radius = radius;
    slot_it->arc_radius = next_arc_radius;
    slot_it->sweep_angle = next_sweep;
    if (start_angle != 0.0) {
      slot_it->start_angle = start_angle;
    }
  } else {
    validate_slot_parameters(length, radius, "straight");
    slot_it->center_x = center_x;
    slot_it->center_y = center_y;
    slot_it->length = length;
    slot_it->radius = radius;
    slot_it->rotation = rotation;
  }

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
