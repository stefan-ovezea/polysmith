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
  return {
      {"kerf_width_mm", laser.kerf_width_mm},
      {"lead_in_mm", laser.lead_in_mm},
      {"lead_out_mm", laser.lead_out_mm},
      {"pierce_dwell_seconds", laser.pierce_dwell_seconds},
      {"power_percent", laser.power_percent},
      {"passes", laser.passes},
      {"mode", laser.mode},
      {"material_thickness_mm", laser.material_thickness_mm},
      {"cut_plane_offset_mm", laser.cut_plane_offset_mm},
      {"dynamic_power", laser.dynamic_power},
  };
}

// ── GeometryReference ────────────────────────────────────────────

json to_payload(const polysmith::core::GeometryReference& ref) {
  json j;
  j["persistent_id"] = ref.persistent_id;
  std::visit([&](const auto& att) { j["attestation"] = to_payload(att); },
             ref.attestation);
  j["fallback_strategy"] = ref.fallback_strategy;
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

// ── CamPointLocation ─────────────────────────────────────────────

json to_payload(const polysmith::core::CamPointLocation& loc) {
  return {
      {"vertex_id", loc.vertex_id},
      {"position", array3(loc.position)},
      {"surface_normal", array3(loc.surface_normal)},
      {"hole_diameter", loc.hole_diameter.has_value()
           ? json(loc.hole_diameter.value()) : json(nullptr)},
      {"sample_face", to_payload(loc.sample_face)},
      {"fallback_strategy", loc.fallback_strategy},
  };
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
      {"tool_id", op.tool_id},
      {"geometry_references", to_payload(op.geometry_references)},
      {"parameters", to_payload(op.parameters)},
      {"dependencies", to_payload(op.dependencies)},
      {"status", op.status},
      {"status_message", op.status_message},
  };
  json pl = json::array();
  for (const auto& loc : op.point_locations) pl.push_back(to_payload(loc));
  j["point_locations"] = pl;
  if (op.toolpath_cache.has_value())
    j["toolpath_cache"] = to_payload(op.toolpath_cache.value());
  else
    j["toolpath_cache"] = nullptr;
  return j;
}

// ── PostProcessor ────────────────────────────────────────────────

json to_payload(const polysmith::core::PostProcessorOptions& opt) {
  json j{
      {"add_line_numbers", opt.add_line_numbers},
      {"use_arcs", opt.use_arcs},
      {"absolute_coordinates", opt.absolute_coordinates},
      {"tool_change_mcode", opt.tool_change_mcode},
      {"spindle_start_mcode", opt.spindle_start_mcode},
      {"coolant_mcode_on", opt.coolant_mcode_on},
      {"coolant_mcode_off", opt.coolant_mcode_off},
      {"decimal_places", opt.decimal_places},
  };
  if (opt.separate_rapids.has_value()) j["separate_rapids"] = opt.separate_rapids.value();
  if (opt.header_string.has_value())   j["header_string"]   = opt.header_string.value();
  if (opt.footer_string.has_value())   j["footer_string"]   = opt.footer_string.value();
  return j;
}

json to_payload(const polysmith::core::PostProcessor& pp) {
  return {
      {"type", pp.type},
      {"filename", pp.filename},
      {"options", to_payload(pp.options)},
  };
}

// ── SimulationData ───────────────────────────────────────────────

json to_payload(const polysmith::core::CollisionReport& cr) {
  return {
      {"op_id", cr.op_id},
      {"tool_id", cr.tool_id},
      {"position", array3(cr.position)},
      {"severity", cr.severity},
      {"message", cr.message},
  };
}

json to_payload(const polysmith::core::SimulationData& sim) {
  json j{{"collisions_detected", sim.collisions_detected}};
  if (sim.stock_after_op_id.has_value())   j["stock_after_op_id"]   = sim.stock_after_op_id.value();
  if (sim.stock_mesh_reference.has_value()) j["stock_mesh_reference"] = sim.stock_mesh_reference.value();
  if (sim.last_verification.has_value())    j["last_verification"]    = sim.last_verification.value();
  json reports = json::array();
  for (const auto& cr : sim.collision_report) reports.push_back(to_payload(cr));
  j["collision_report"] = reports;
  return j;
}

// ── CamDocumentData ──────────────────────────────────────────────

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

  j["simulation"] = cam.simulation.has_value()
      ? to_payload(cam.simulation.value()) : json(nullptr);

  return j;
}
