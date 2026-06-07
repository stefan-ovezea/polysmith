#pragma once

#include <cmath>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace polysmith::core {

struct BodyAppearanceOverride {
  std::string body_id;
  std::string color;
};

struct FaceAppearanceOverride {
  std::string face_id;
  std::string owner_body_id;
  std::string signature;
  std::string color;
};

struct DocumentAppearance {
  std::vector<BodyAppearanceOverride> body_colors;
  std::vector<FaceAppearanceOverride> face_colors;
};

inline bool is_valid_appearance_color(const std::string& color) {
  if (color.size() != 7 || color[0] != '#') {
    return false;
  }
  for (std::size_t i = 1; i < color.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(color[i]))) {
      return false;
    }
  }
  return true;
}

inline std::optional<std::string> owner_body_id_from_face_id(
    const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos || separator == 0) {
    return std::nullopt;
  }
  return face_id.substr(0, separator);
}

inline std::string appearance_face_signature(const TopoDS_Face& face) {
  if (face.IsNull()) {
    return "";
  }

  BRepAdaptor_Surface surface(face);
  const double u = (surface.FirstUParameter() + surface.LastUParameter()) / 2.0;
  const double v = (surface.FirstVParameter() + surface.LastVParameter()) / 2.0;
  gp_Pnt point;
  gp_Vec du;
  gp_Vec dv;
  try {
    surface.D1(u, v, point, du, dv);
  } catch (...) {
    return "";
  }

  gp_Vec normal = du.Crossed(dv);
  if (normal.Magnitude() <= 0.0) {
    return "";
  }
  normal.Normalize();

  auto rounded = [](double value) {
    return std::round(value * 1000.0) / 1000.0;
  };

  std::ostringstream stream;
  stream << static_cast<int>(surface.GetType()) << ":" << std::fixed
         << std::setprecision(3) << rounded(normal.X()) << ":"
         << rounded(normal.Y()) << ":" << rounded(normal.Z());
  return stream.str();
}

}  // namespace polysmith::core
