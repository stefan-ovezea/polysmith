polysmith::core::FeatureEntry feature_entry_from_payload(const json& payload) {
  polysmith::core::FeatureEntry feature{};
  feature.id = read_string(payload, "feature_id");
  feature.kind = read_string(payload, "kind");
  feature.name = read_string(payload, "name");
  feature.status = read_string(payload, "status");
  feature.parameters_summary = read_string(payload, "parameters_summary");
  // Older `.polysmith` files predate the `suppressed` flag — treat
  // missing/non-bool fields as false so loading them is non-fatal.
  if (payload.contains("suppressed") && payload.at("suppressed").is_boolean()) {
    feature.suppressed = payload.at("suppressed").get<bool>();
  }
  // Older saves predate dependency tracking — default to clean state.
  // The first geometry mutation after load will recompute the flag
  // anyway via `refresh_history_dependencies`.
  if (payload.contains("dependency_broken") &&
      payload.at("dependency_broken").is_boolean()) {
    feature.dependency_broken = payload.at("dependency_broken").get<bool>();
  }
  if (payload.contains("dependency_warning") &&
      payload.at("dependency_warning").is_string()) {
    feature.dependency_warning =
        payload.at("dependency_warning").get<std::string>();
  }

  if (payload.contains("box_parameters") &&
      !payload.at("box_parameters").is_null()) {
    const json& box_payload = payload.at("box_parameters");
    polysmith::core::BoxFeatureParameters box{};
    box.width = read_number(box_payload, "width");
    box.height = read_number(box_payload, "height");
    box.depth = read_number(box_payload, "depth");
    feature.box_parameters = box;
  }

  if (payload.contains("cylinder_parameters") &&
      !payload.at("cylinder_parameters").is_null()) {
    const json& cyl_payload = payload.at("cylinder_parameters");
    polysmith::core::CylinderFeatureParameters cylinder{};
    cylinder.radius = read_number(cyl_payload, "radius");
    cylinder.height = read_number(cyl_payload, "height");
    feature.cylinder_parameters = cylinder;
  }

  if (payload.contains("extrude_parameters") &&
      !payload.at("extrude_parameters").is_null()) {
    feature.extrude_parameters =
        parse_extrude_parameters_from_payload(payload.at("extrude_parameters"));
  }

  if (payload.contains("loft_parameters") &&
      !payload.at("loft_parameters").is_null()) {
    feature.loft_parameters =
        loft_parameters_from_payload(payload.at("loft_parameters"));
  }

  if (payload.contains("revolve_parameters") &&
      !payload.at("revolve_parameters").is_null()) {
    feature.revolve_parameters =
        revolve_parameters_from_payload(payload.at("revolve_parameters"));
  }

  if (payload.contains("sweep_parameters") &&
      !payload.at("sweep_parameters").is_null()) {
    feature.sweep_parameters =
        sweep_parameters_from_payload(payload.at("sweep_parameters"));
  }

  if (payload.contains("sketch_parameters") &&
      !payload.at("sketch_parameters").is_null()) {
    feature.sketch_parameters =
        sketch_parameters_from_payload(payload.at("sketch_parameters"));
  }

  if (payload.contains("fillet_parameters") &&
      !payload.at("fillet_parameters").is_null()) {
    const json& fp = payload.at("fillet_parameters");
    polysmith::core::FilletFeatureParameters params{};
    params.target_body_id = read_string(fp, "target_body_id");
    if (fp.contains("edge_ids") && fp.at("edge_ids").is_array()) {
      for (const auto& edge : fp.at("edge_ids")) {
        params.edge_ids.push_back(edge.get<std::string>());
      }
    }
    params.radius = read_number(fp, "radius");
    // Default false when absent so older saved documents (created
    // before is_pending existed) load with the same semantics they
    // had at save time — i.e. "already confirmed".
    if (fp.contains("is_pending") && fp.at("is_pending").is_boolean()) {
      params.is_pending = fp.at("is_pending").get<bool>();
    }
    feature.fillet_parameters = params;
  }

  if (payload.contains("chamfer_parameters") &&
      !payload.at("chamfer_parameters").is_null()) {
    const json& cp = payload.at("chamfer_parameters");
    polysmith::core::ChamferFeatureParameters params{};
    params.target_body_id = read_string(cp, "target_body_id");
    if (cp.contains("edge_ids") && cp.at("edge_ids").is_array()) {
      for (const auto& edge : cp.at("edge_ids")) {
        params.edge_ids.push_back(edge.get<std::string>());
      }
    }
    params.distance = read_number(cp, "distance");
    if (cp.contains("is_pending") && cp.at("is_pending").is_boolean()) {
      params.is_pending = cp.at("is_pending").get<bool>();
    }
    feature.chamfer_parameters = params;
  }

  if (payload.contains("shell_parameters") &&
      !payload.at("shell_parameters").is_null()) {
    const json& sp = payload.at("shell_parameters");
    polysmith::core::ShellFeatureParameters params{};
    params.target_body_id = read_string(sp, "target_body_id");
    if (sp.contains("removed_face_ids") &&
        sp.at("removed_face_ids").is_array()) {
      for (const auto& face : sp.at("removed_face_ids")) {
        params.removed_face_ids.push_back(face.get<std::string>());
      }
    }
    params.thickness = read_number(sp, "thickness");
    if (sp.contains("is_pending") && sp.at("is_pending").is_boolean()) {
      params.is_pending = sp.at("is_pending").get<bool>();
    }
    feature.shell_parameters = params;
  }

  if (payload.contains("construction_plane_parameters") &&
      !payload.at("construction_plane_parameters").is_null()) {
    const json& cpp = payload.at("construction_plane_parameters");
    polysmith::core::ConstructionPlaneFeatureParameters params{};
    if (cpp.contains("plane_type") && cpp.at("plane_type").is_string()) {
      params.plane_type = cpp.at("plane_type").get<std::string>();
    }
    params.source_plane_id = read_string(cpp, "source_plane_id");
    if (cpp.contains("source_plane_ids") && cpp.at("source_plane_ids").is_array()) {
      for (const auto& id : cpp.at("source_plane_ids")) {
        if (id.is_string()) {
          params.source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (params.source_plane_ids.empty() && !params.source_plane_id.empty()) {
      params.source_plane_ids.push_back(params.source_plane_id);
    }
    if (cpp.contains("source_axis_id") && cpp.at("source_axis_id").is_string()) {
      params.source_axis_id = cpp.at("source_axis_id").get<std::string>();
    }
    params.offset = read_number(cpp, "offset");
    if (cpp.contains("angle_degrees") && cpp.at("angle_degrees").is_number()) {
      params.angle_degrees = cpp.at("angle_degrees").get<double>();
    }
    if (cpp.contains("plane_frame") && cpp.at("plane_frame").is_object()) {
      params.plane_frame = plane_frame_from_payload(cpp.at("plane_frame"));
    }
    feature.construction_plane_parameters = params;
  }

  if (payload.contains("construction_axis_parameters") &&
      !payload.at("construction_axis_parameters").is_null()) {
    const json& cap = payload.at("construction_axis_parameters");
    polysmith::core::ConstructionAxisFeatureParameters params{};
    params.source_id = read_string(cap, "source_id");
    params.source_kind =
        read_optional_string_value(cap, "source_kind", "edge");
    params.start_x = require(cap, "start").at("x").get<double>();
    params.start_y = require(cap, "start").at("y").get<double>();
    params.start_z = require(cap, "start").at("z").get<double>();
    params.end_x = require(cap, "end").at("x").get<double>();
    params.end_y = require(cap, "end").at("y").get<double>();
    params.end_z = require(cap, "end").at("z").get<double>();
    feature.construction_axis_parameters = params;
  }

  if (payload.contains("construction_point_parameters") &&
      !payload.at("construction_point_parameters").is_null()) {
    const json& cpp = payload.at("construction_point_parameters");
    polysmith::core::ConstructionPointFeatureParameters params{};
    params.source_id = read_string(cpp, "source_id");
    params.source_kind =
        read_optional_string_value(cpp, "source_kind", "vertex");
    params.x = require(cpp, "position").at("x").get<double>();
    params.y = require(cpp, "position").at("y").get<double>();
    params.z = require(cpp, "position").at("z").get<double>();
    feature.construction_point_parameters = params;
  }

  if (payload.contains("hole_parameters") &&
      !payload.at("hole_parameters").is_null()) {
    const json& hp = payload.at("hole_parameters");
    polysmith::core::HoleFeatureParameters params{};
    params.target_body_id = read_string(hp, "target_body_id");
    params.source_face_id = read_string(hp, "source_face_id");
    params.plane_frame = plane_frame_from_payload(require(hp, "plane_frame"));
    params.center_x = read_number(hp, "center_x");
    params.center_y = read_number(hp, "center_y");
    params.hole_type = read_optional_string_value(hp, "hole_type", "simple");
    params.extent_type = read_optional_string_value(hp, "extent_type", "blind");
    params.diameter = read_optional_number(hp, "diameter", params.diameter);
    params.depth = read_optional_number(hp, "depth", params.depth);
    params.counterbore_diameter = read_optional_number(
        hp, "counterbore_diameter", params.counterbore_diameter);
    params.counterbore_depth = read_optional_number(
        hp, "counterbore_depth", params.counterbore_depth);
    params.countersink_diameter = read_optional_number(
        hp, "countersink_diameter", params.countersink_diameter);
    params.countersink_angle_degrees = read_optional_number(
        hp, "countersink_angle_degrees", params.countersink_angle_degrees);
    params.standard =
        read_optional_string_value(hp, "standard", params.standard);
    params.standard_size =
        read_optional_string_value(hp, "standard_size", params.standard_size);
    params.hole_fit =
        read_optional_string_value(hp, "hole_fit", params.hole_fit);
    if (hp.contains("thread_enabled") && hp.at("thread_enabled").is_boolean()) {
      params.thread_enabled = hp.at("thread_enabled").get<bool>();
    }
    params.thread_spec =
        read_optional_string_value(hp, "thread_spec", params.thread_spec);
    params.thread_pitch =
        read_optional_number(hp, "thread_pitch", params.thread_pitch);
    params.major_diameter =
        read_optional_number(hp, "major_diameter", params.major_diameter);
    params.minor_diameter =
        read_optional_number(hp, "minor_diameter", params.minor_diameter);
    params.thread_depth =
        read_optional_number(hp, "thread_depth", params.thread_depth);
    params.thread_representation = read_optional_string_value(
        hp, "thread_representation", params.thread_representation);
    if (hp.contains("is_pending") && hp.at("is_pending").is_boolean()) {
      params.is_pending = hp.at("is_pending").get<bool>();
    }
    feature.hole_parameters = params;
  }

  if (payload.contains("helix_parameters") &&
      !payload.at("helix_parameters").is_null()) {
    const json& hp = payload.at("helix_parameters");
    polysmith::core::HelixFeatureParameters params{};
    params.axis_source_id = read_string(hp, "axis_source_id");
    params.axis_start_x = require(hp, "axis_start").at("x").get<double>();
    params.axis_start_y = require(hp, "axis_start").at("y").get<double>();
    params.axis_start_z = require(hp, "axis_start").at("z").get<double>();
    params.axis_end_x = require(hp, "axis_end").at("x").get<double>();
    params.axis_end_y = require(hp, "axis_end").at("y").get<double>();
    params.axis_end_z = require(hp, "axis_end").at("z").get<double>();
    params.radius = read_optional_number(hp, "radius", params.radius);
    params.pitch = read_optional_number(hp, "pitch", params.pitch);
    params.height = read_optional_number(hp, "height", params.height);
    params.turns = read_optional_number(hp, "turns", params.turns);
    params.handedness =
        read_optional_string_value(hp, "handedness", params.handedness);
    params.start_angle_degrees = read_optional_number(
        hp, "start_angle_degrees", params.start_angle_degrees);
    if (hp.contains("points") && hp.at("points").is_array()) {
      for (const auto& point_value : hp.at("points")) {
        if (point_value.is_number()) {
          params.points.push_back(point_value.get<double>());
        }
      }
    }
    feature.helix_parameters = params;
  }

  if (payload.contains("thread_parameters") &&
      !payload.at("thread_parameters").is_null()) {
    const json& tp = payload.at("thread_parameters");
    polysmith::core::ThreadFeatureParameters params{};
    params.target_body_id =
        read_optional_string_value(tp, "target_body_id", params.target_body_id);
    params.axis_source_id =
        read_optional_string_value(tp, "axis_source_id", params.axis_source_id);
    params.mode = read_optional_string_value(tp, "mode", params.mode);
    params.standard =
        read_optional_string_value(tp, "standard", params.standard);
    params.size = read_optional_string_value(tp, "size", params.size);
    params.major_diameter =
        read_optional_number(tp, "major_diameter", params.major_diameter);
    params.minor_diameter =
        read_optional_number(tp, "minor_diameter", params.minor_diameter);
    params.pitch = read_optional_number(tp, "pitch", params.pitch);
    params.length = read_optional_number(tp, "length", params.length);
    params.thread_angle_degrees = read_optional_number(
        tp, "thread_angle_degrees", params.thread_angle_degrees);
    params.start_offset =
        read_optional_number(tp, "start_offset", params.start_offset);
    params.handedness =
        read_optional_string_value(tp, "handedness", params.handedness);
    params.representation =
        read_optional_string_value(tp, "representation", params.representation);
    if (tp.contains("is_pending") && tp.at("is_pending").is_boolean()) {
      params.is_pending = tp.at("is_pending").get<bool>();
    }
    feature.thread_parameters = params;
  }

  if (payload.contains("fastener_parameters") &&
      !payload.at("fastener_parameters").is_null()) {
    const json& fp = payload.at("fastener_parameters");
    polysmith::core::FastenerFeatureParameters params{};
    params.standard =
        read_optional_string_value(fp, "standard", params.standard);
    params.size = read_optional_string_value(fp, "size", params.size);
    params.diameter = read_optional_number(fp, "diameter", params.diameter);
    params.minor_diameter =
        read_optional_number(fp, "minor_diameter", params.minor_diameter);
    params.pitch = read_optional_number(fp, "pitch", params.pitch);
    params.length = read_optional_number(fp, "length", params.length);
    params.thread_length =
        read_optional_number(fp, "thread_length", params.thread_length);
    params.head_type =
        read_optional_string_value(fp, "head_type", params.head_type);
    params.drive_type =
        read_optional_string_value(fp, "drive_type", params.drive_type);
    params.thread_representation = read_optional_string_value(
        fp, "thread_representation", params.thread_representation);
    feature.fastener_parameters = params;
  }

  if (payload.contains("move_parameters") &&
      !payload.at("move_parameters").is_null()) {
    const json& mp = payload.at("move_parameters");
    polysmith::core::MoveFeatureParameters params{};
    params.target_body_id =
        read_optional_string_value(mp, "target_body_id", params.target_body_id);
    params.translation_x =
        read_optional_number(mp, "translation_x", params.translation_x);
    params.translation_y =
        read_optional_number(mp, "translation_y", params.translation_y);
    params.translation_z =
        read_optional_number(mp, "translation_z", params.translation_z);
    params.rotation_x_degrees =
        read_optional_number(mp, "rotation_x_degrees", params.rotation_x_degrees);
    params.rotation_y_degrees =
        read_optional_number(mp, "rotation_y_degrees", params.rotation_y_degrees);
    params.rotation_z_degrees =
        read_optional_number(mp, "rotation_z_degrees", params.rotation_z_degrees);
    if (mp.contains("is_pending") && mp.at("is_pending").is_boolean()) {
      params.is_pending = mp.at("is_pending").get<bool>();
    }
    feature.move_parameters = params;
  }

  if (payload.contains("body_copy_parameters") &&
      !payload.at("body_copy_parameters").is_null()) {
    const json& bp = payload.at("body_copy_parameters");
    polysmith::core::BodyCopyFeatureParameters params{};
    params.source_body_id =
        read_optional_string_value(bp, "source_body_id", params.source_body_id);
    params.copy_mode =
        read_optional_string_value(bp, "copy_mode", params.copy_mode);
    params.source_body_name =
        read_optional_string_value(bp, "source_body_name", params.source_body_name);
    params.serialized_shape =
        read_optional_string_value(bp, "serialized_shape", params.serialized_shape);
    params.local_x_axis_x =
        read_optional_number(bp, "local_x_axis_x", params.local_x_axis_x);
    params.local_x_axis_y =
        read_optional_number(bp, "local_x_axis_y", params.local_x_axis_y);
    params.local_x_axis_z =
        read_optional_number(bp, "local_x_axis_z", params.local_x_axis_z);
    params.local_y_axis_x =
        read_optional_number(bp, "local_y_axis_x", params.local_y_axis_x);
    params.local_y_axis_y =
        read_optional_number(bp, "local_y_axis_y", params.local_y_axis_y);
    params.local_y_axis_z =
        read_optional_number(bp, "local_y_axis_z", params.local_y_axis_z);
    params.local_z_axis_x =
        read_optional_number(bp, "local_z_axis_x", params.local_z_axis_x);
    params.local_z_axis_y =
        read_optional_number(bp, "local_z_axis_y", params.local_z_axis_y);
    params.local_z_axis_z =
        read_optional_number(bp, "local_z_axis_z", params.local_z_axis_z);
    feature.body_copy_parameters = params;
  }

  return feature;
}
