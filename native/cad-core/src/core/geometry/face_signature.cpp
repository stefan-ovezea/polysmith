#include "core/geometry/face_signature.h"

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace polysmith::core {

std::string face_signature(const TopoDS_Face& face) {
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
