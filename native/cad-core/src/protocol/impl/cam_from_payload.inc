// ── CAM Deserialization ──────────────────────────────────────────
//
// Inverses of the CAM `to_payload` functions above.  Loaded documents
// carry the "cam" key; everything here mirrors the target-schema types
// in cam_types.h.  Missing keys fall back to the struct defaults so
// documents saved before a field existed still load (same convention
// as document_from_payload's parameter/appearance parsing).
//
// Note: toolpaths are memory-only by design — nothing here reads or
// writes toolpath point arrays.  The serialized ToolpathCache carries
// metadata only.

namespace {

double cam_read_number(const json& payload, const char* key,
                       double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<double>();
}

int cam_read_int(const json& payload, const char* key, int fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<int>();
}

bool cam_read_bool(const json& payload, const char* key, bool fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<bool>();
}

std::string cam_read_string(const json& payload, const char* key,
                            const std::string& fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<std::string>();
}

std::array<double, 3> cam_read_array3(const json& payload, const char* key,
                                      std::array<double, 3> fallback) {
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return fallback;
  }
  const auto& arr = payload.at(key);
  if (arr.size() < 3) {
    return fallback;
  }
  return {arr.at(0).get<double>(), arr.at(1).get<double>(),
          arr.at(2).get<double>()};
}

std::vector<std::array<double, 3>> cam_read_array3_list(
    const json& payload, const char* key) {
  std::vector<std::array<double, 3>> result;
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return result;
  }
  for (const auto& entry : payload.at(key)) {
    if (entry.is_array() && entry.size() >= 3) {
      result.push_back({entry.at(0).get<double>(), entry.at(1).get<double>(),
                        entry.at(2).get<double>()});
    }
  }
  return result;
}

std::vector<std::string> cam_read_string_list(const json& payload,
                                              const char* key) {
  std::vector<std::string> result;
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return result;
  }
  for (const auto& entry : payload.at(key)) {
    if (entry.is_string()) {
      result.push_back(entry.get<std::string>());
    }
  }
  return result;
}

}  // namespace

polysmith::core::Bounds3D bounds_from_payload(const json& payload) {
  polysmith::core::Bounds3D b;
  b.min_x = cam_read_number(payload, "min_x", 0.0);
  b.min_y = cam_read_number(payload, "min_y", 0.0);
  b.min_z = cam_read_number(payload, "min_z", 0.0);
  b.max_x = cam_read_number(payload, "max_x", 0.0);
  b.max_y = cam_read_number(payload, "max_y", 0.0);
  b.max_z = cam_read_number(payload, "max_z", 0.0);
  return b;
}

polysmith::core::FaceAttestation face_attestation_from_payload(
    const json& payload) {
  polysmith::core::FaceAttestation att;
  if (payload.contains("bounds")) {
    att.bounds = bounds_from_payload(payload.at("bounds"));
  }
  att.area = cam_read_number(payload, "area", 0.0);
  att.normal = cam_read_array3(payload, "normal", {0.0, 0.0, 1.0});
  att.sample_points = cam_read_array3_list(payload, "sample_points");
  return att;
}

polysmith::core::EdgeAttestation edge_attestation_from_payload(
    const json& payload) {
  polysmith::core::EdgeAttestation att;
  att.start_point = cam_read_array3(payload, "start_point", {0.0, 0.0, 0.0});
  att.end_point = cam_read_array3(payload, "end_point", {0.0, 0.0, 0.0});
  att.length = cam_read_number(payload, "length", 0.0);
  att.tangent = cam_read_array3(payload, "tangent", {1.0, 0.0, 0.0});
  if (payload.contains("adjacent_face_normals")) {
    att.adjacent_face_normals =
        cam_read_array3_list(payload, "adjacent_face_normals");
  }
  return att;
}

polysmith::core::SketchProfileAttestation
sketch_profile_attestation_from_payload(const json& payload) {
  polysmith::core::SketchProfileAttestation att;
  att.sketch_feature_id = cam_read_string(payload, "sketch_feature_id", "");
  att.profile_id = cam_read_string(payload, "profile_id", "");
  att.center_x = cam_read_number(payload, "center_x", 0.0);
  att.center_y = cam_read_number(payload, "center_y", 0.0);
  att.area = cam_read_number(payload, "area", 0.0);
  att.min_x = cam_read_number(payload, "min_x", 0.0);
  att.min_y = cam_read_number(payload, "min_y", 0.0);
  att.max_x = cam_read_number(payload, "max_x", 0.0);
  att.max_y = cam_read_number(payload, "max_y", 0.0);
  att.boundary_edge_kinds = cam_read_string_list(payload, "boundary_edge_kinds");
  att.inner_loop_count = cam_read_int(payload, "inner_loop_count", 0);
  if (payload.contains("source_circle_id") &&
      !payload.at("source_circle_id").is_null()) {
    att.source_circle_id =
        payload.at("source_circle_id").get<std::string>();
  }
  return att;
}

// Discriminates the attestation variant by which witness keys the
// payload carries: sketch refs have sketch_feature_id, face refs have
// sample_points, edge refs have start_point/tangent.
polysmith::core::GeometryReference geometry_reference_from_payload(
    const json& payload) {
  polysmith::core::GeometryReference ref;
  ref.persistent_id = cam_read_string(payload, "persistent_id", "");
  if (!payload.contains("attestation")) {
    // Degenerate reference without attestation — resolution will fail
    // with a human-readable message; never crash on load.
    ref.attestation = polysmith::core::FaceAttestation{};
    return ref;
  }
  const auto& att = payload.at("attestation");
  if (att.contains("sketch_feature_id")) {
    ref.attestation = sketch_profile_attestation_from_payload(att);
  } else if (att.contains("sample_points")) {
    ref.attestation = face_attestation_from_payload(att);
  } else {
    ref.attestation = edge_attestation_from_payload(att);
  }
  return ref;
}

polysmith::core::StockDefinition stock_definition_from_payload(
    const json& payload) {
  polysmith::core::StockDefinition s;
  s.type = cam_read_string(payload, "type", "bounding_box");
  if (payload.contains("origin") && !payload.at("origin").is_null()) {
    s.origin = cam_read_array3(payload, "origin", {0.0, 0.0, 0.0});
  }
  if (payload.contains("size") && !payload.at("size").is_null()) {
    s.size = cam_read_array3(payload, "size", {0.0, 0.0, 0.0});
  }
  if (payload.contains("diameter") && !payload.at("diameter").is_null()) {
    s.diameter = cam_read_number(payload, "diameter", 0.0);
  }
  if (payload.contains("length") && !payload.at("length").is_null()) {
    s.length = cam_read_number(payload, "length", 0.0);
  }
  s.margin = cam_read_number(payload, "margin", 3.0);
  if (payload.contains("solid_reference") &&
      !payload.at("solid_reference").is_null()) {
    s.solid_reference =
        geometry_reference_from_payload(payload.at("solid_reference"));
  }
  if (payload.contains("mesh_reference") &&
      !payload.at("mesh_reference").is_null()) {
    s.mesh_reference = payload.at("mesh_reference").get<std::string>();
  }
  return s;
}

polysmith::core::MachineAxes machine_axes_from_payload(const json& payload) {
  polysmith::core::MachineAxes ma;
  ma.x = cam_read_number(payload, "x", 500.0);
  ma.y = cam_read_number(payload, "y", 400.0);
  ma.z = cam_read_number(payload, "z", 300.0);
  if (payload.contains("a") && !payload.at("a").is_null()) {
    ma.a = cam_read_number(payload, "a", 0.0);
  }
  if (payload.contains("b") && !payload.at("b").is_null()) {
    ma.b = cam_read_number(payload, "b", 0.0);
  }
  if (payload.contains("c") && !payload.at("c").is_null()) {
    ma.c = cam_read_number(payload, "c", 0.0);
  }
  return ma;
}

polysmith::core::WcsOrigin wcs_origin_from_payload(const json& payload) {
  polysmith::core::WcsOrigin wcs;
  wcs.feature_id = cam_read_string(payload, "feature_id", "");
  if (payload.contains("face_reference")) {
    wcs.face_reference =
        geometry_reference_from_payload(payload.at("face_reference"));
  }
  if (payload.contains("position") && !payload.at("position").is_null()) {
    wcs.position = cam_read_array3(payload, "position", {0.0, 0.0, 0.0});
  }
  return wcs;
}

polysmith::core::CamSetup cam_setup_from_payload(const json& payload) {
  polysmith::core::CamSetup setup;
  setup.setup_id = cam_read_string(payload, "setup_id", "");
  setup.name = cam_read_string(payload, "name", "");
  setup.machine_type = cam_read_string(payload, "machine_type", "3_axis_mill");
  if (payload.contains("machine_axes")) {
    setup.machine_axes = machine_axes_from_payload(payload.at("machine_axes"));
  }
  if (payload.contains("stock")) {
    setup.stock = stock_definition_from_payload(payload.at("stock"));
  }
  if (payload.contains("wcs_origin")) {
    setup.wcs_origin = wcs_origin_from_payload(payload.at("wcs_origin"));
  }
  setup.safety_height = cam_read_number(payload, "safety_height", 50.0);
  setup.retract_height = cam_read_number(payload, "retract_height", 5.0);
  setup.units = cam_read_string(payload, "units", "mm");
  return setup;
}

polysmith::core::ToolEntry tool_entry_from_payload(const json& payload) {
  polysmith::core::ToolEntry tool;
  tool.tool_id = cam_read_string(payload, "tool_id", "");
  tool.name = cam_read_string(payload, "name", "");
  tool.type = cam_read_string(payload, "type", "endmill_flat");
  tool.diameter_mm = cam_read_number(payload, "diameter_mm", 6.0);
  tool.corner_radius_mm = cam_read_number(payload, "corner_radius_mm", 0.0);
  tool.flute_length_mm = cam_read_number(payload, "flute_length_mm", 20.0);
  tool.overall_length_mm = cam_read_number(payload, "overall_length_mm", 60.0);
  tool.shank_diameter_mm = cam_read_number(payload, "shank_diameter_mm", 6.0);
  tool.material = cam_read_string(payload, "material", "carbide");
  if (payload.contains("coating") && !payload.at("coating").is_null()) {
    tool.coating = payload.at("coating").get<std::string>();
  }
  tool.coolant_through = cam_read_bool(payload, "coolant_through", false);
  tool.max_spindle_rpm = cam_read_number(payload, "max_spindle_rpm", 15000.0);
  tool.default_feedrate_mm_per_min =
      cam_read_number(payload, "default_feedrate_mm_per_min", 1000.0);
  tool.default_plunge_feedrate_mm_per_min =
      cam_read_number(payload, "default_plunge_feedrate_mm_per_min", 500.0);
  tool.default_stepdown_mm = cam_read_number(payload, "default_stepdown_mm", 1.0);
  tool.default_stepover_percent =
      cam_read_number(payload, "default_stepover_percent", 50.0);
  return tool;
}

polysmith::core::CamGeometryReferences cam_geometry_references_from_payload(
    const json& payload) {
  polysmith::core::CamGeometryReferences geo;
  auto read_refs = [](const json& payload, const char* key) {
    std::vector<polysmith::core::GeometryReference> result;
    if (!payload.contains(key) || !payload.at(key).is_array()) {
      return result;
    }
    for (const auto& entry : payload.at(key)) {
      result.push_back(geometry_reference_from_payload(entry));
    }
    return result;
  };
  geo.machining_regions = read_refs(payload, "machining_regions");
  geo.avoidance_regions = read_refs(payload, "avoidance_regions");
  geo.guide_curves = read_refs(payload, "guide_curves");
  geo.check_surfaces = read_refs(payload, "check_surfaces");
  return geo;
}

polysmith::core::LaserCutParameters laser_cut_parameters_from_payload(
    const json& payload) {
  polysmith::core::LaserCutParameters laser;
  laser.mode = cam_read_string(payload, "mode", "cut");
  laser.power_percent = cam_read_number(payload, "power_percent", 85.0);
  laser.passes = cam_read_int(payload, "passes", 1);
  laser.dynamic_power = cam_read_bool(payload, "dynamic_power", true);
  laser.air_assist = cam_read_bool(payload, "air_assist", false);
  laser.kerf_width_mm = cam_read_number(payload, "kerf_width_mm", 0.15);
  laser.kerf_side = cam_read_string(payload, "kerf_side", "auto");
  laser.lead_in_mm = cam_read_number(payload, "lead_in_mm", 2.0);
  laser.lead_out_mm = cam_read_number(payload, "lead_out_mm", 2.0);
  laser.lead_in_style = cam_read_string(payload, "lead_in_style", "line");
  laser.lead_out_style = cam_read_string(payload, "lead_out_style", "line");
  laser.lead_in_angle_deg =
      cam_read_number(payload, "lead_in_angle_deg", 0.0);
  laser.lead_out_angle_deg =
      cam_read_number(payload, "lead_out_angle_deg", 0.0);
  laser.overcut_mm = cam_read_number(payload, "overcut_mm", 0.0);
  laser.pierce_dwell_seconds =
      cam_read_number(payload, "pierce_dwell_seconds", 0.1);
  laser.pierce_position = cam_read_string(payload, "pierce_position", "auto");
  laser.tabs_enabled = cam_read_bool(payload, "tabs_enabled", false);
  laser.tab_width_mm = cam_read_number(payload, "tab_width_mm", 0.5);
  laser.tab_spacing_mm = cam_read_number(payload, "tab_spacing_mm", 20.0);
  laser.tab_power_percent =
      cam_read_number(payload, "tab_power_percent", 0.0);
  laser.tabs_on_holes = cam_read_bool(payload, "tabs_on_holes", false);
  laser.engrave_style = cam_read_string(payload, "engrave_style", "line");
  laser.line_spacing_mm = cam_read_number(payload, "line_spacing_mm", 0.1);
  laser.fill_angle_deg = cam_read_number(payload, "fill_angle_deg", 0.0);
  laser.fill_bidirectional =
      cam_read_bool(payload, "fill_bidirectional", true);
  laser.material_thickness_mm =
      cam_read_number(payload, "material_thickness_mm", 3.0);
  laser.cut_plane_offset_mm =
      cam_read_number(payload, "cut_plane_offset_mm", 0.0);
  laser.cut_order = cam_read_string(payload, "cut_order", "inner_first");
  if (payload.contains("speed_mm_per_s") &&
      !payload.at("speed_mm_per_s").is_null()) {
    laser.speed_mm_per_s = cam_read_number(payload, "speed_mm_per_s", 0.0);
  }
  return laser;
}

polysmith::core::CamOperationParameters cam_operation_parameters_from_payload(
    const json& payload) {
  polysmith::core::CamOperationParameters p;
  p.spindle_rpm = cam_read_number(payload, "spindle_rpm", 8000.0);
  p.feedrate_mm_per_min =
      cam_read_number(payload, "feedrate_mm_per_min", 1200.0);
  p.plunge_feedrate_mm_per_min =
      cam_read_number(payload, "plunge_feedrate_mm_per_min", 600.0);
  p.stock_allowance_mm = cam_read_number(payload, "stock_allowance_mm", 0.2);
  p.cutting_direction = cam_read_string(payload, "cutting_direction", "climb");
  p.finish_pass = cam_read_bool(payload, "finish_pass", false);
  p.multiple_passes = cam_read_bool(payload, "multiple_passes", false);
  p.coolant = cam_read_string(payload, "coolant", "off");
  if (payload.contains("stepdown_mm") && !payload.at("stepdown_mm").is_null()) {
    p.stepdown_mm = cam_read_number(payload, "stepdown_mm", 1.0);
  }
  if (payload.contains("stepover_percent") &&
      !payload.at("stepover_percent").is_null()) {
    p.stepover_percent = cam_read_number(payload, "stepover_percent", 50.0);
  }
  if (payload.contains("strategy") && !payload.at("strategy").is_null()) {
    p.strategy = payload.at("strategy").get<std::string>();
  }
  if (payload.contains("cycle_type") && !payload.at("cycle_type").is_null()) {
    p.cycle_type = payload.at("cycle_type").get<std::string>();
  }
  if (payload.contains("hole_depth_mm") &&
      !payload.at("hole_depth_mm").is_null()) {
    p.hole_depth_mm = cam_read_number(payload, "hole_depth_mm", 0.0);
  }
  if (payload.contains("peck_depth_mm") &&
      !payload.at("peck_depth_mm").is_null()) {
    p.peck_depth_mm = cam_read_number(payload, "peck_depth_mm", 0.0);
  }
  if (payload.contains("dwell_seconds") &&
      !payload.at("dwell_seconds").is_null()) {
    p.dwell_seconds = cam_read_number(payload, "dwell_seconds", 0.0);
  }
  if (payload.contains("engagement_angle_deg") &&
      !payload.at("engagement_angle_deg").is_null()) {
    p.engagement_angle_deg =
        cam_read_number(payload, "engagement_angle_deg", 0.0);
  }
  if (payload.contains("zigzag_angle_deg") &&
      !payload.at("zigzag_angle_deg").is_null()) {
    p.zigzag_angle_deg = cam_read_number(payload, "zigzag_angle_deg", 0.0);
  }
  if (payload.contains("laser") && !payload.at("laser").is_null()) {
    p.laser = laser_cut_parameters_from_payload(payload.at("laser"));
  }
  if (payload.contains("test_pattern") &&
      !payload.at("test_pattern").is_null()) {
    p.test_pattern =
        laser_test_pattern_parameters_from_payload(payload.at("test_pattern"));
  }
  return p;
}

polysmith::core::LaserTestPatternParameters
laser_test_pattern_parameters_from_payload(const json& payload) {
  polysmith::core::LaserTestPatternParameters pattern;
  pattern.pattern = cam_read_string(payload, "pattern", "engrave_grid");
  pattern.power_min_percent =
      cam_read_number(payload, "power_min_percent", 10.0);
  pattern.power_max_percent =
      cam_read_number(payload, "power_max_percent", 100.0);
  pattern.power_steps = cam_read_int(payload, "power_steps", 5);
  pattern.speed_min_mm_per_s =
      cam_read_number(payload, "speed_min_mm_per_s", 5.0);
  pattern.speed_max_mm_per_s =
      cam_read_number(payload, "speed_max_mm_per_s", 50.0);
  pattern.speed_steps = cam_read_int(payload, "speed_steps", 5);
  pattern.cell_size_mm = cam_read_number(payload, "cell_size_mm", 10.0);
  pattern.cell_spacing_mm =
      cam_read_number(payload, "cell_spacing_mm", 5.0);
  pattern.start_x_mm = cam_read_number(payload, "start_x_mm", 5.0);
  pattern.start_y_mm = cam_read_number(payload, "start_y_mm", 5.0);
  pattern.line_spacing_mm =
      cam_read_number(payload, "line_spacing_mm", 0.1);
  pattern.kerf_width_mm = cam_read_number(payload, "kerf_width_mm", 0.15);
  pattern.power_percent = cam_read_number(payload, "power_percent", 85.0);
  pattern.speed_mm_per_s = cam_read_number(payload, "speed_mm_per_s", 10.0);
  pattern.cell_labels = cam_read_bool(payload, "cell_labels", true);
  return pattern;
}

polysmith::core::LaserMachineSettings laser_machine_settings_from_payload(
    const json& payload) {
  polysmith::core::LaserMachineSettings machine;
  machine.work_area_x_mm = cam_read_number(payload, "work_area_x_mm", 400.0);
  machine.work_area_y_mm = cam_read_number(payload, "work_area_y_mm", 400.0);
  machine.pointer_offset_x_mm =
      cam_read_number(payload, "pointer_offset_x_mm", 0.0);
  machine.pointer_offset_y_mm =
      cam_read_number(payload, "pointer_offset_y_mm", 0.0);
  return machine;
}

polysmith::core::CamOperationDependencies cam_operation_dependencies_from_payload(
    const json& payload) {
  polysmith::core::CamOperationDependencies d;
  d.parent_operation_ids = cam_read_string_list(payload, "parent_operation_ids");
  if (payload.contains("requires_operation_id") &&
      !payload.at("requires_operation_id").is_null()) {
    d.requires_operation_id =
        payload.at("requires_operation_id").get<std::string>();
  }
  d.use_stock_from_previous =
      cam_read_bool(payload, "use_stock_from_previous", false);
  return d;
}

polysmith::core::ExternalStorage external_storage_from_payload(
    const json& payload) {
  polysmith::core::ExternalStorage es;
  es.format = cam_read_string(payload, "format", "binary");
  es.file_reference = cam_read_string(payload, "file_reference", "");
  es.compressed = cam_read_bool(payload, "compressed", false);
  if (payload.contains("size_bytes") && !payload.at("size_bytes").is_null()) {
    es.size_bytes =
        static_cast<int64_t>(cam_read_number(payload, "size_bytes", 0.0));
  }
  return es;
}

polysmith::core::ToolpathCache toolpath_cache_from_payload(
    const json& payload) {
  polysmith::core::ToolpathCache tc;
  tc.toolpath_id = cam_read_string(payload, "toolpath_id", "");
  tc.status = cam_read_string(payload, "status", "up_to_date");
  if (payload.contains("generated_at") &&
      !payload.at("generated_at").is_null()) {
    tc.generated_at = payload.at("generated_at").get<std::string>();
  }
  if (payload.contains("bounds") && !payload.at("bounds").is_null()) {
    tc.bounds = bounds_from_payload(payload.at("bounds"));
  }
  if (payload.contains("total_length_mm") &&
      !payload.at("total_length_mm").is_null()) {
    tc.total_length_mm = cam_read_number(payload, "total_length_mm", 0.0);
  }
  if (payload.contains("estimated_time_seconds") &&
      !payload.at("estimated_time_seconds").is_null()) {
    tc.estimated_time_seconds =
        cam_read_number(payload, "estimated_time_seconds", 0.0);
  }
  if (payload.contains("num_moves") && !payload.at("num_moves").is_null()) {
    tc.num_moves = cam_read_int(payload, "num_moves", 0);
  }
  if (payload.contains("num_rapids") && !payload.at("num_rapids").is_null()) {
    tc.num_rapids = cam_read_int(payload, "num_rapids", 0);
  }
  if (payload.contains("num_feeds") && !payload.at("num_feeds").is_null()) {
    tc.num_feeds = cam_read_int(payload, "num_feeds", 0);
  }
  if (payload.contains("external_storage") &&
      !payload.at("external_storage").is_null()) {
    tc.external_storage =
        external_storage_from_payload(payload.at("external_storage"));
  }
  return tc;
}

polysmith::core::CamOperation cam_operation_from_payload(const json& payload) {
  polysmith::core::CamOperation op;
  op.op_id = cam_read_string(payload, "op_id", "");
  op.name = cam_read_string(payload, "name", "");
  op.type = cam_read_string(payload, "type", "face_milling");
  op.enabled = cam_read_bool(payload, "enabled", true);
  op.setup_id = cam_read_string(payload, "setup_id", "");
  op.tool_id = cam_read_string(payload, "tool_id", "");
  if (payload.contains("geometry_references")) {
    op.geometry_references =
        cam_geometry_references_from_payload(payload.at("geometry_references"));
  }
  if (payload.contains("parameters")) {
    op.parameters = cam_operation_parameters_from_payload(payload.at("parameters"));
  }
  if (payload.contains("dependencies")) {
    op.dependencies =
        cam_operation_dependencies_from_payload(payload.at("dependencies"));
  }
  if (payload.contains("toolpath_cache") &&
      !payload.at("toolpath_cache").is_null()) {
    op.toolpath_cache =
        toolpath_cache_from_payload(payload.at("toolpath_cache"));
  }
  op.status = cam_read_string(payload, "status", "pending");
  op.status_message = cam_read_string(payload, "status_message", "");
  return op;
}

polysmith::core::PostProcessor post_processor_from_payload(const json& payload) {
  polysmith::core::PostProcessor pp;
  pp.type = cam_read_string(payload, "type", "fanuc");
  pp.filename = cam_read_string(payload, "filename", "");
  if (payload.contains("options")) {
  }
  return pp;
}

polysmith::core::CamDocumentData cam_document_data_from_payload(
    const json& payload) {
  polysmith::core::CamDocumentData cam;
  if (payload.contains("setups") && payload.at("setups").is_array()) {
    for (const auto& setup : payload.at("setups")) {
      cam.setups.push_back(cam_setup_from_payload(setup));
    }
  }
  if (payload.contains("tool_library") &&
      payload.at("tool_library").is_array()) {
    for (const auto& tool : payload.at("tool_library")) {
      cam.tool_library.push_back(tool_entry_from_payload(tool));
    }
  }
  if (payload.contains("operations") && payload.at("operations").is_array()) {
    for (const auto& op : payload.at("operations")) {
      cam.operations.push_back(cam_operation_from_payload(op));
    }
  }
  if (payload.contains("post_processor") &&
      !payload.at("post_processor").is_null()) {
    cam.post_processor =
        post_processor_from_payload(payload.at("post_processor"));
  }
  if (payload.contains("machine_settings") &&
      !payload.at("machine_settings").is_null()) {
    cam.machine_settings =
        laser_machine_settings_from_payload(payload.at("machine_settings"));
  }
  return cam;
}
