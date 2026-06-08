const json& require(const json& payload, const char* key) {
  if (!payload.is_object() || !payload.contains(key)) {
    throw std::runtime_error(std::string("Missing field: ") + key);
  }
  return payload.at(key);
}

std::string read_string(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_string()) {
    throw std::runtime_error(std::string("Field is not a string: ") + key);
  }
  return value.get<std::string>();
}

double read_number(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_number()) {
    throw std::runtime_error(std::string("Field is not a number: ") + key);
  }
  return value.get<double>();
}

int read_int(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_number_integer() && !value.is_number()) {
    throw std::runtime_error(std::string("Field is not an integer: ") + key);
  }
  return value.get<int>();
}

bool read_bool(const json& payload, const char* key) {
  const json& value = require(payload, key);
  if (!value.is_boolean()) {
    throw std::runtime_error(std::string("Field is not a bool: ") + key);
  }
  return value.get<bool>();
}

std::optional<std::string> read_optional_string(const json& payload,
                                                const char* key) {
  if (!payload.is_object() || !payload.contains(key) || payload.at(key).is_null()) {
    return std::nullopt;
  }
  return payload.at(key).get<std::string>();
}

double read_optional_number(const json& payload,
                            const char* key,
                            double fallback) {
  if (!payload.is_object() || !payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Field is not a number: ") + key);
  }
  return payload.at(key).get<double>();
}

std::string read_optional_string_value(const json& payload,
                                       const char* key,
                                       const std::string& fallback) {
  if (!payload.is_object() || !payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Field is not a string: ") + key);
  }
  return payload.at(key).get<std::string>();
}

polysmith::core::PlaneFrame plane_frame_from_payload(const json& payload) {
  polysmith::core::PlaneFrame frame{};
  frame.origin_x = require(payload, "origin").at("x").get<double>();
  frame.origin_y = require(payload, "origin").at("y").get<double>();
  frame.origin_z = require(payload, "origin").at("z").get<double>();
  frame.x_axis_x = require(payload, "x_axis").at("x").get<double>();
  frame.x_axis_y = require(payload, "x_axis").at("y").get<double>();
  frame.x_axis_z = require(payload, "x_axis").at("z").get<double>();
  frame.y_axis_x = require(payload, "y_axis").at("x").get<double>();
  frame.y_axis_y = require(payload, "y_axis").at("y").get<double>();
  frame.y_axis_z = require(payload, "y_axis").at("z").get<double>();
  frame.normal_x = require(payload, "normal").at("x").get<double>();
  frame.normal_y = require(payload, "normal").at("y").get<double>();
  frame.normal_z = require(payload, "normal").at("z").get<double>();
  return frame;
}

polysmith::core::ExtrudeFeatureParameters::SideParameters
extrude_side_from_payload(
    const json& payload,
    const polysmith::core::ExtrudeFeatureParameters::SideParameters& fallback) {
  polysmith::core::ExtrudeFeatureParameters::SideParameters side = fallback;
  if (!payload.is_object()) {
    return side;
  }
  side.extent_type =
      read_optional_string_value(payload, "extent_type", side.extent_type);
  side.distance = read_optional_number(payload, "distance", side.distance);
  side.start_offset =
      read_optional_number(payload, "start_offset", side.start_offset);
  side.taper_angle_degrees =
      read_optional_number(payload,
                           "taper_angle_degrees",
                           side.taper_angle_degrees);
  side.target_reference_id = read_optional_string(payload, "target_reference_id");
  return side;
}

json extrude_side_to_payload(
    const polysmith::core::ExtrudeFeatureParameters::SideParameters& side) {
  return json{
      {"extent_type", side.extent_type},
      {"distance", side.distance},
      {"start_offset", side.start_offset},
      {"taper_angle_degrees", side.taper_angle_degrees},
      {"target_reference_id",
       side.target_reference_id.has_value()
           ? json(side.target_reference_id.value())
           : json(nullptr)},
  };
}

polysmith::core::SketchFeatureParameters::SketchPlaneFrame
sketch_plane_frame_from_payload(const json& payload) {
  polysmith::core::SketchFeatureParameters::SketchPlaneFrame frame{};
  const polysmith::core::PlaneFrame base = plane_frame_from_payload(payload);
  frame.origin_x = base.origin_x;
  frame.origin_y = base.origin_y;
  frame.origin_z = base.origin_z;
  frame.x_axis_x = base.x_axis_x;
  frame.x_axis_y = base.x_axis_y;
  frame.x_axis_z = base.x_axis_z;
  frame.y_axis_x = base.y_axis_x;
  frame.y_axis_y = base.y_axis_y;
  frame.y_axis_z = base.y_axis_z;
  frame.normal_x = base.normal_x;
  frame.normal_y = base.normal_y;
  frame.normal_z = base.normal_z;
  return frame;
}

json viewport_sketch_plane_frame_to_payload(
    const polysmith::core::ViewportSketchPlaneFrame& frame) {
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
