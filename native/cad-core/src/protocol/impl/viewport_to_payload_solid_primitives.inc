json to_payload(const polysmith::core::ViewportBoxPrimitive& primitive) {
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"width", primitive.width},
      {"height", primitive.height},
      {"depth", primitive.depth},
      {"x_offset", primitive.x_offset},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
      }},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportCylinderPrimitive& primitive) {
  json plane_frame_json = nullptr;
  if (primitive.plane_frame.has_value()) {
    plane_frame_json = {
        {"origin",
         {
             {"x", primitive.plane_frame->origin_x},
             {"y", primitive.plane_frame->origin_y},
             {"z", primitive.plane_frame->origin_z},
         }},
        {"x_axis",
         {
             {"x", primitive.plane_frame->x_axis_x},
             {"y", primitive.plane_frame->x_axis_y},
             {"z", primitive.plane_frame->x_axis_z},
         }},
        {"y_axis",
         {
             {"x", primitive.plane_frame->y_axis_x},
             {"y", primitive.plane_frame->y_axis_y},
             {"z", primitive.plane_frame->y_axis_z},
         }},
        {"normal",
         {
             {"x", primitive.plane_frame->normal_x},
             {"y", primitive.plane_frame->normal_y},
             {"z", primitive.plane_frame->normal_z},
         }},
    };
  }
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"radius", primitive.radius},
      {"height", primitive.height},
      {"x_offset", primitive.x_offset},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
      }},
      {"plane_id", primitive.plane_id},
      {"plane_frame", plane_frame_json},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportPolygonExtrudePrimitive& primitive) {
  json profile_points = json::array();
  for (const auto& point : primitive.profile_points) {
    profile_points.push_back({
        {"x", point.x},
        {"y", point.y},
    });
  }
  json inner_loops = json::array();
  for (const auto& loop : primitive.inner_loops) {
    json loop_payload = json::array();
    for (const auto& point : loop) {
      loop_payload.push_back({
          {"x", point.x},
          {"y", point.y},
      });
    }
    inner_loops.push_back(loop_payload);
  }

  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? json{
                 {"origin",
                  {
                      {"x", primitive.plane_frame->origin_x},
                      {"y", primitive.plane_frame->origin_y},
                      {"z", primitive.plane_frame->origin_z},
                  }},
                 {"x_axis",
                  {
                      {"x", primitive.plane_frame->x_axis_x},
                      {"y", primitive.plane_frame->x_axis_y},
                      {"z", primitive.plane_frame->x_axis_z},
                  }},
                 {"y_axis",
                  {
                      {"x", primitive.plane_frame->y_axis_x},
                      {"y", primitive.plane_frame->y_axis_y},
                      {"z", primitive.plane_frame->y_axis_z},
                  }},
                 {"normal",
                  {
                      {"x", primitive.plane_frame->normal_x},
                      {"y", primitive.plane_frame->normal_y},
                      {"z", primitive.plane_frame->normal_z},
                  }},
             }
           : json(nullptr)},
      {"profile_points", profile_points},
      {"depth", primitive.depth},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportSolidFace& face) {
  // Legacy single-face form, kept for callers that need one face (there are
  // none today: the state payload uses the packed table below). The field set
  // matches what the UI expects for an individual face.
  json cylinder_axis_json = nullptr;
  if (face.cylinder_axis.has_value()) {
    cylinder_axis_json = {face.cylinder_axis->at(0), face.cylinder_axis->at(1),
                          face.cylinder_axis->at(2)};
  }
  json cylinder_location_json = nullptr;
  if (face.cylinder_location.has_value()) {
    cylinder_location_json = {face.cylinder_location->at(0),
                              face.cylinder_location->at(1),
                              face.cylinder_location->at(2)};
  }
  return {
      {"face_id", face.face_id},
      {"owner_id", face.owner_id},
      {"owner_kind", face.owner_kind},
      {"label", face.label},
      {"sketchability", face.sketchability},
      {"surface_kind", face.surface_kind},
      {"cylinder_axis", cylinder_axis_json},
      {"cylinder_location", cylinder_location_json},
      {"cylinder_radius",
       face.cylinder_radius.has_value() ? json(face.cylinder_radius.value())
                                        : json(nullptr)},
      {"center",
       {
           {"x", face.center_x},
           {"y", face.center_y},
           {"z", face.center_z},
       }},
      {"normal",
       {
           {"x", face.normal_x},
           {"y", face.normal_y},
           {"z", face.normal_z},
       }},
      {"plane_frame",
       {
           {"origin",
            {
                {"x", face.plane_frame.origin_x},
                {"y", face.plane_frame.origin_y},
                {"z", face.plane_frame.origin_z},
            }},
           {"x_axis",
            {
                {"x", face.plane_frame.x_axis_x},
                {"y", face.plane_frame.x_axis_y},
                {"z", face.plane_frame.x_axis_z},
            }},
           {"y_axis",
            {
                {"x", face.plane_frame.y_axis_x},
                {"y", face.plane_frame.y_axis_y},
                {"z", face.plane_frame.y_axis_z},
            }},
           {"normal",
            {
                {"x", face.plane_frame.normal_x},
                {"y", face.plane_frame.normal_y},
                {"z", face.plane_frame.normal_z},
            }},
       }},
      {"size",
       {
           {"width", face.width},
           {"height", face.height},
           {"radius", face.radius},
       }},
      {"is_selected", face.is_selected},
      {"appearance_color",
       face.appearance_color.has_value()
           ? json(face.appearance_color.value())
           : json(nullptr)},
  };
}

// Packed face table.
//
// A face used to cross the wire as an object with nested `center`/`normal`/
// `plane_frame`/`size` objects and six strings, which cost ~670 B/face on a
// 14,897-face board (9.54 MB, of which 3.25 MB was the keyed plane frame and
// most of the rest was the same ids and enum strings repeated 14,897 times).
//
// Here the repetition is gone: strings are interned into five small tables,
// enums are indices, and the numbers are flat float32 blobs with ONE decode on
// the UI. `face_id` and `label` are not sent at all — both are derivable from
// the owner and the ordinal ("<owner>:face:<ordinal>", "Face <ordinal+1>").
json to_payload(const std::vector<polysmith::core::ViewportSolidFace>& faces) {
  std::vector<std::string> owner_ids;
  std::vector<std::string> owner_kinds;
  std::vector<std::string> sketchabilities;
  std::vector<std::string> surface_kinds;
  std::vector<std::string> colors;

  const auto intern = [](std::vector<std::string>& table,
                         const std::string& value) -> int {
    for (std::size_t index = 0; index < table.size(); ++index) {
      if (table[index] == value) {
        return static_cast<int>(index);
      }
    }
    table.push_back(value);
    return static_cast<int>(table.size()) - 1;
  };

  std::vector<int> owner;
  std::vector<int> ordinal;
  std::vector<int> sketchability;
  std::vector<int> surface_kind;
  std::vector<int> appearance;
  std::vector<int> flags;
  std::vector<double> center;
  std::vector<double> normal;
  std::vector<double> frame;
  std::vector<double> size;
  std::vector<double> cylinder;

  owner.reserve(faces.size());
  ordinal.reserve(faces.size());
  sketchability.reserve(faces.size());
  surface_kind.reserve(faces.size());
  appearance.reserve(faces.size());
  flags.reserve(faces.size());
  center.reserve(faces.size() * 3);
  normal.reserve(faces.size() * 3);
  frame.reserve(faces.size() * 12);
  size.reserve(faces.size() * 3);

  for (const auto& face : faces) {
    const int owner_index = intern(owner_ids, face.owner_id);
    // Keep owner_kinds parallel to owner_ids: a body's kind is constant.
    if (static_cast<std::size_t>(owner_index) >= owner_kinds.size()) {
      owner_kinds.resize(static_cast<std::size_t>(owner_index) + 1);
      owner_kinds[static_cast<std::size_t>(owner_index)] = face.owner_kind;
    }
    owner.push_back(owner_index);

    // The ordinal is the trailing number of "<owner>:face:<ordinal>".
    int face_ordinal = 0;
    const std::size_t colon = face.face_id.rfind(':');
    if (colon != std::string::npos) {
      try {
        face_ordinal = std::stoi(face.face_id.substr(colon + 1));
      } catch (const std::exception&) {
        face_ordinal = 0;
      }
    }
    ordinal.push_back(face_ordinal);
    sketchability.push_back(intern(sketchabilities, face.sketchability));
    surface_kind.push_back(intern(surface_kinds, face.surface_kind));
    appearance.push_back(face.appearance_color.has_value()
                             ? intern(colors, face.appearance_color.value())
                             : -1);

    int face_flags = 0;
    if (face.is_selected) {
      face_flags |= 1;
    }
    if (face.cylinder_axis.has_value() && face.cylinder_location.has_value() &&
        face.cylinder_radius.has_value()) {
      face_flags |= 2;
    }
    flags.push_back(face_flags);

    center.push_back(face.center_x);
    center.push_back(face.center_y);
    center.push_back(face.center_z);
    normal.push_back(face.normal_x);
    normal.push_back(face.normal_y);
    normal.push_back(face.normal_z);
    frame.push_back(face.plane_frame.origin_x);
    frame.push_back(face.plane_frame.origin_y);
    frame.push_back(face.plane_frame.origin_z);
    frame.push_back(face.plane_frame.x_axis_x);
    frame.push_back(face.plane_frame.x_axis_y);
    frame.push_back(face.plane_frame.x_axis_z);
    frame.push_back(face.plane_frame.y_axis_x);
    frame.push_back(face.plane_frame.y_axis_y);
    frame.push_back(face.plane_frame.y_axis_z);
    frame.push_back(face.plane_frame.normal_x);
    frame.push_back(face.plane_frame.normal_y);
    frame.push_back(face.plane_frame.normal_z);
    size.push_back(face.width);
    size.push_back(face.height);
    size.push_back(face.radius);

    if ((face_flags & 2) != 0) {
      cylinder.push_back(face.cylinder_axis->at(0));
      cylinder.push_back(face.cylinder_axis->at(1));
      cylinder.push_back(face.cylinder_axis->at(2));
      cylinder.push_back(face.cylinder_location->at(0));
      cylinder.push_back(face.cylinder_location->at(1));
      cylinder.push_back(face.cylinder_location->at(2));
      cylinder.push_back(face.cylinder_radius.value());
    }
  }

  return {
      {"count", faces.size()},
      {"owner_ids", owner_ids},
      {"owner_kinds", owner_kinds},
      {"sketchabilities", sketchabilities},
      {"surface_kinds", surface_kinds},
      {"colors", colors},
      {"owner", encode_u32(owner)},
      {"ordinal", encode_u32(ordinal)},
      {"sketchability", encode_u32(sketchability)},
      {"surface_kind", encode_u32(surface_kind)},
      // -1 marks "no override"; read back as a signed int on the UI.
      {"appearance", encode_u32(appearance)},
      {"flags", encode_u32(flags)},
      {"center", encode_f32(center)},
      {"normal", encode_f32(normal)},
      {"frame", encode_f32(frame)},
      {"size", encode_f32(size)},
      {"cylinder", encode_f32(cylinder)},
  };
}
