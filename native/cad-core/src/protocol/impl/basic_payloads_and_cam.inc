polysmith::core::ExtrudeFeatureParameters
extrude_parameters_from_payload(const json& payload) {
  return parse_extrude_parameters_from_payload(payload);
}

json plane_frame_to_payload(const polysmith::core::PlaneFrame& frame) {
  return {
      {"origin",
       {
           {"x", frame.origin_x},
           {"y", frame.origin_y},
           {"z", frame.origin_z},
       }},
      {"x_axis",
       {
           {"x", frame.x_axis_x},
           {"y", frame.x_axis_y},
           {"z", frame.x_axis_z},
       }},
      {"y_axis",
       {
           {"x", frame.y_axis_x},
           {"y", frame.y_axis_y},
           {"z", frame.y_axis_z},
       }},
      {"normal",
       {
           {"x", frame.normal_x},
           {"y", frame.normal_y},
           {"z", frame.normal_z},
       }},
  };
}

json profile_points_to_payload(
    const std::vector<polysmith::core::SketchProfilePoint>& points) {
  json payload = json::array();
  for (const auto& point : points) {
    payload.push_back({{"x", point.x}, {"y", point.y}});
  }
  return payload;
}

// ══════════════════════════════════════════════════════════════════
//  CAM Serialization  (target-schema types from cam_types.h)
// ══════════════════════════════════════════════════════════════════

namespace {

json array3(const std::array<double, 3>& a) {
  return {a[0], a[1], a[2]};
}

std::array<double, 3> to_array3(const json& j) {
  return {j.at(0).get<double>(), j.at(1).get<double>(), j.at(2).get<double>()};
}

}  // namespace

// ── Bounds3D ─────────────────────────────────────────────────────

json to_payload(const polysmith::core::Bounds3D& b) {
  return {
      {"min_x", b.min_x}, {"min_y", b.min_y}, {"min_z", b.min_z},
      {"max_x", b.max_x}, {"max_y", b.max_y}, {"max_z", b.max_z}};
}

// ── FaceAttestation / EdgeAttestation ────────────────────────────

json to_payload(const polysmith::core::FaceAttestation& att) {
  json j;
  j["bounds"]  = to_payload(att.bounds);
  j["area"]    = att.area;
  j["normal"]  = array3(att.normal);
  json sp = json::array();
  for (const auto& p : att.sample_points) sp.push_back(array3(p));
  j["sample_points"] = sp;
  return j;
}

json to_payload(const polysmith::core::EdgeAttestation& att) {
  json j;
  j["start_point"] = array3(att.start_point);
  j["end_point"]   = array3(att.end_point);
  j["length"]      = att.length;
  j["tangent"]     = array3(att.tangent);
  if (att.adjacent_face_normals.has_value()) {
    json nf = json::array();
    for (const auto& n : att.adjacent_face_normals.value())
      nf.push_back(array3(n));
    j["adjacent_face_normals"] = nf;
  }
  return j;
}

json to_payload(const polysmith::core::SketchProfileAttestation& att) {
  json j{
      {"sketch_feature_id", att.sketch_feature_id},
      {"profile_id", att.profile_id},
      {"center_x", att.center_x},
      {"center_y", att.center_y},
      {"area", att.area},
      {"min_x", att.min_x},
      {"min_y", att.min_y},
      {"max_x", att.max_x},
      {"max_y", att.max_y},
      {"boundary_edge_kinds", att.boundary_edge_kinds},
      {"inner_loop_count", att.inner_loop_count},
  };
  if (att.source_circle_id.has_value())
    j["source_circle_id"] = att.source_circle_id.value();
  return j;
}

json to_payload(const polysmith::core::LaserCutParameters& laser) {
  json j = {
      {"mode", laser.mode},
      {"power_percent", laser.power_percent},
      {"passes", laser.passes},
      {"dynamic_power", laser.dynamic_power},
      {"air_assist", laser.air_assist},
      {"kerf_width_mm", laser.kerf_width_mm},
      {"kerf_side", laser.kerf_side},
      {"lead_in_mm", laser.lead_in_mm},
      {"lead_out_mm", laser.lead_out_mm},
      {"lead_in_style", laser.lead_in_style},
      {"lead_out_style", laser.lead_out_style},
      {"lead_in_angle_deg", laser.lead_in_angle_deg},
      {"lead_out_angle_deg", laser.lead_out_angle_deg},
      {"overcut_mm", laser.overcut_mm},
      {"pierce_dwell_seconds", laser.pierce_dwell_seconds},
      {"pierce_position", laser.pierce_position},
      {"tabs_enabled", laser.tabs_enabled},
      {"tab_width_mm", laser.tab_width_mm},
      {"tab_spacing_mm", laser.tab_spacing_mm},
      {"tab_power_percent", laser.tab_power_percent},
      {"tabs_on_holes", laser.tabs_on_holes},
      {"engrave_style", laser.engrave_style},
      {"line_spacing_mm", laser.line_spacing_mm},
      {"fill_angle_deg", laser.fill_angle_deg},
      {"fill_bidirectional", laser.fill_bidirectional},
      {"material_thickness_mm", laser.material_thickness_mm},
      {"cut_plane_offset_mm", laser.cut_plane_offset_mm},
      {"cut_order", laser.cut_order},
      {"arc_segments_per_circle", laser.arc_segments_per_circle},
  };
  if (laser.speed_mm_per_s.has_value()) {
    j["speed_mm_per_s"] = laser.speed_mm_per_s.value();
  }
  if (laser.pierce_angle_deg.has_value()) {
    j["pierce_angle_deg"] = laser.pierce_angle_deg.value();
  }
  return j;
}

// ── GeometryReference ────────────────────────────────────────────

json to_payload(const polysmith::core::GeometryReference& ref) {
  json j;
  j["persistent_id"] = ref.persistent_id;
  std::visit([&](const auto& att) { j["attestation"] = to_payload(att); },
             ref.attestation);
  return j;
}

// ── StockDefinition ──────────────────────────────────────────────

json to_payload(const polysmith::core::StockDefinition& s) {
  json j;
  j["type"] = s.type;
  if (s.origin.has_value()) j["origin"] = array3(s.origin.value());
  if (s.size.has_value())   j["size"]   = array3(s.size.value());
  if (s.diameter.has_value()) j["diameter"] = s.diameter.value();
  if (s.length.has_value())   j["length"]   = s.length.value();
  j["margin"] = s.margin;
  if (s.solid_reference.has_value())
    j["solid_reference"] = to_payload(s.solid_reference.value());
  if (s.mesh_reference.has_value())
    j["mesh_reference"] = s.mesh_reference.value();
  return j;
}

// ── MachineAxes ──────────────────────────────────────────────────

json to_payload(const polysmith::core::MachineAxes& ma) {
  json j{{"x", ma.x}, {"y", ma.y}, {"z", ma.z}};
  if (ma.a.has_value()) j["a"] = ma.a.value();
  if (ma.b.has_value()) j["b"] = ma.b.value();
  if (ma.c.has_value()) j["c"] = ma.c.value();
  return j;
}

// ── WcsOrigin ────────────────────────────────────────────────────

json to_payload(const polysmith::core::WcsOrigin& wcs) {
  json j{
      {"feature_id", wcs.feature_id},
      {"face_reference", to_payload(wcs.face_reference)}};
  if (wcs.position.has_value()) j["position"] = array3(wcs.position.value());
  return j;
}

// ── CamSetup ─────────────────────────────────────────────────────

json to_payload(const polysmith::core::CamSetup& setup) {
  return {
      {"setup_id", setup.setup_id},
      {"name", setup.name},
      {"machine_type", setup.machine_type},
      {"machine_axes", to_payload(setup.machine_axes)},
      {"stock", to_payload(setup.stock)},
      {"wcs_origin", to_payload(setup.wcs_origin)},
      {"safety_height", setup.safety_height},
      {"retract_height", setup.retract_height},
      {"units", setup.units},
  };
}

// ── ToolEntry ────────────────────────────────────────────────────

json to_payload(const polysmith::core::ToolEntry& tool) {
  json j{
      {"tool_id", tool.tool_id},
      {"name", tool.name},
      {"type", tool.type},
      {"diameter_mm", tool.diameter_mm},
      {"corner_radius_mm", tool.corner_radius_mm},
      {"flute_length_mm", tool.flute_length_mm},
      {"overall_length_mm", tool.overall_length_mm},
      {"shank_diameter_mm", tool.shank_diameter_mm},
      {"material", tool.material},
      {"coolant_through", tool.coolant_through},
      {"max_spindle_rpm", tool.max_spindle_rpm},
      {"default_feedrate_mm_per_min", tool.default_feedrate_mm_per_min},
      {"default_plunge_feedrate_mm_per_min", tool.default_plunge_feedrate_mm_per_min},
      {"default_stepdown_mm", tool.default_stepdown_mm},
      {"default_stepover_percent", tool.default_stepover_percent},
  };
  if (tool.coating.has_value()) j["coating"] = tool.coating.value();
  return j;
}

// ── CamGeometryReferences ────────────────────────────────────────

json to_payload(const polysmith::core::CamGeometryReferences& geo) {
  json j;
  auto arr = [](const auto& v) {
    json a = json::array();
    for (const auto& r : v) a.push_back(to_payload(r));
    return a;
  };
  j["machining_regions"] = arr(geo.machining_regions);
  j["avoidance_regions"] = arr(geo.avoidance_regions);
  j["guide_curves"]      = arr(geo.guide_curves);
  j["check_surfaces"]    = arr(geo.check_surfaces);
  return j;
}

// ── CamOperationParameters ───────────────────────────────────────

json to_payload(const polysmith::core::CamOperationParameters& p) {
  json j{
      {"spindle_rpm", p.spindle_rpm},
      {"feedrate_mm_per_min", p.feedrate_mm_per_min},
      {"plunge_feedrate_mm_per_min", p.plunge_feedrate_mm_per_min},
      {"stock_allowance_mm", p.stock_allowance_mm},
      {"cutting_direction", p.cutting_direction},
      {"finish_pass", p.finish_pass},
      {"multiple_passes", p.multiple_passes},
      {"coolant", p.coolant},
  };
  if (p.stepdown_mm.has_value())         j["stepdown_mm"]         = p.stepdown_mm.value();
  if (p.stepover_percent.has_value())    j["stepover_percent"]    = p.stepover_percent.value();
  if (p.strategy.has_value())            j["strategy"]            = p.strategy.value();
  if (p.cycle_type.has_value())          j["cycle_type"]          = p.cycle_type.value();
  if (p.hole_depth_mm.has_value())       j["hole_depth_mm"]       = p.hole_depth_mm.value();
  if (p.peck_depth_mm.has_value())       j["peck_depth_mm"]       = p.peck_depth_mm.value();
  if (p.dwell_seconds.has_value())       j["dwell_seconds"]       = p.dwell_seconds.value();
  if (p.engagement_angle_deg.has_value()) j["engagement_angle_deg"] = p.engagement_angle_deg.value();
  if (p.zigzag_angle_deg.has_value())     j["zigzag_angle_deg"]     = p.zigzag_angle_deg.value();
  if (p.laser.has_value())                j["laser"]                = to_payload(p.laser.value());
  if (p.test_pattern.has_value())         j["test_pattern"]         = to_payload(p.test_pattern.value());
  return j;
}

// ── CamOperationDependencies ─────────────────────────────────────

json to_payload(const polysmith::core::CamOperationDependencies& d) {
  return {
      {"parent_operation_ids", d.parent_operation_ids},
      {"requires_operation_id", d.requires_operation_id.has_value()
           ? json(d.requires_operation_id.value()) : json(nullptr)},
      {"use_stock_from_previous", d.use_stock_from_previous},
  };
}

// ── ExternalStorage ──────────────────────────────────────────────

json to_payload(const polysmith::core::ExternalStorage& es) {
  json j{
      {"format", es.format},
      {"file_reference", es.file_reference},
      {"compressed", es.compressed},
  };
  if (es.size_bytes.has_value()) j["size_bytes"] = es.size_bytes.value();
  return j;
}

// ── ToolpathCache ────────────────────────────────────────────────

json to_payload(const polysmith::core::ToolpathCache& tc) {
  json j{
      {"toolpath_id", tc.toolpath_id},
      {"status", tc.status},
  };
  if (tc.generated_at.has_value())         j["generated_at"]         = tc.generated_at.value();
  if (tc.bounds.has_value())               j["bounds"]               = to_payload(tc.bounds.value());
  if (tc.total_length_mm.has_value())      j["total_length_mm"]      = tc.total_length_mm.value();
  if (tc.estimated_time_seconds.has_value()) j["estimated_time_seconds"] = tc.estimated_time_seconds.value();
  if (tc.num_moves.has_value())            j["num_moves"]            = tc.num_moves.value();
  if (tc.num_rapids.has_value())           j["num_rapids"]           = tc.num_rapids.value();
  if (tc.num_feeds.has_value())            j["num_feeds"]            = tc.num_feeds.value();
  if (tc.external_storage.has_value())     j["external_storage"]     = to_payload(tc.external_storage.value());
  return j;
}

// ── CamOperation ─────────────────────────────────────────────────

json to_payload(const polysmith::core::CamOperation& op) {
  json j{
      {"op_id", op.op_id},
      {"name", op.name},
      {"type", op.type},
      {"enabled", op.enabled},
      {"setup_id", op.setup_id},
      {"tool_id", op.tool_id},
      {"geometry_references", to_payload(op.geometry_references)},
      {"parameters", to_payload(op.parameters)},
      {"dependencies", to_payload(op.dependencies)},
      {"status", op.status},
      {"status_message", op.status_message},
  };
  if (op.toolpath_cache.has_value())
    j["toolpath_cache"] = to_payload(op.toolpath_cache.value());
  else
    j["toolpath_cache"] = nullptr;
  return j;
}

// ── PostProcessor ────────────────────────────────────────────────

json to_payload(const polysmith::core::PostProcessor& pp) {
  return {
      {"type", pp.type},
      {"filename", pp.filename},
  };
}

// ── CamDocumentData ──────────────────────────────────────────────

json to_payload(const polysmith::core::LaserMachineSettings& machine) {
  return {
      {"work_area_x_mm", machine.work_area_x_mm},
      {"work_area_y_mm", machine.work_area_y_mm},
      {"pointer_offset_x_mm", machine.pointer_offset_x_mm},
      {"pointer_offset_y_mm", machine.pointer_offset_y_mm},
  };
}

json to_payload(const polysmith::core::LaserTestPatternParameters& pattern) {
  return {
      {"pattern", pattern.pattern},
      {"power_min_percent", pattern.power_min_percent},
      {"power_max_percent", pattern.power_max_percent},
      {"power_steps", pattern.power_steps},
      {"speed_min_mm_per_s", pattern.speed_min_mm_per_s},
      {"speed_max_mm_per_s", pattern.speed_max_mm_per_s},
      {"speed_steps", pattern.speed_steps},
      {"cell_size_mm", pattern.cell_size_mm},
      {"cell_spacing_mm", pattern.cell_spacing_mm},
      {"start_x_mm", pattern.start_x_mm},
      {"start_y_mm", pattern.start_y_mm},
      {"line_spacing_mm", pattern.line_spacing_mm},
      {"kerf_width_mm", pattern.kerf_width_mm},
      {"power_percent", pattern.power_percent},
      {"speed_mm_per_s", pattern.speed_mm_per_s},
      {"cell_labels", pattern.cell_labels},
  };
}

json to_payload(const polysmith::core::CamDocumentData& cam) {
  json j;
  json setups = json::array();
  for (const auto& s : cam.setups) setups.push_back(to_payload(s));
  j["setups"] = setups;

  json tools = json::array();
  for (const auto& t : cam.tool_library) tools.push_back(to_payload(t));
  j["tool_library"] = tools;

  json ops = json::array();
  for (const auto& o : cam.operations) ops.push_back(to_payload(o));
  j["operations"] = ops;

  j["post_processor"] = cam.post_processor.has_value()
      ? to_payload(cam.post_processor.value()) : json(nullptr);

  j["machine_settings"] = cam.machine_settings.has_value()
      ? to_payload(cam.machine_settings.value()) : json(nullptr);

  return j;
}
