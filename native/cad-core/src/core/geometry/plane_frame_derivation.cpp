#include "core/geometry/plane_frame_derivation.h"

#include <cmath>
#include <exception>

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

namespace polysmith::core {

namespace {

// A normal this close to +-Z counts as horizontal. Below it the
// projection of world Z onto the plane is still well conditioned in
// double precision (|Z - (Z.n)n| >= sin(~8e-5 rad) at the threshold).
constexpr double kHorizontalNormalDot = 1.0 - 1.0e-9;

// Unit-length guard for the basis vectors.
constexpr double kAxisEpsilon = 1.0e-12;

bool normalise(double& x, double& y, double& z) {
  const double length = std::sqrt(x * x + y * y + z * z);
  if (!(length > kAxisEpsilon)) {
    return false;
  }
  x /= length;
  y /= length;
  z /= length;
  return true;
}

}  // namespace

std::optional<PlaneFrame> canonical_planar_face_frame(const TopoDS_Face& face) {
  if (face.IsNull()) {
    return std::nullopt;
  }
  try {
    // `false` = do not restrict to the face's parametric bounds; the
    // untrimmed surface is what carries the plane, and skipping the
    // bounds keeps this cheap on converted-mesh bodies.
    BRepAdaptor_Surface surface(face, /*Restriction=*/false);
    if (surface.GetType() != GeomAbs_Plane) {
      return std::nullopt;
    }

    const gp_Pln plane = surface.Plane();
    const gp_Ax3 axes = plane.Position();
    const gp_Pnt origin = axes.Location();
    const gp_Dir plane_normal = axes.Direction();

    // A reversed face has the surface normal pointing into the solid;
    // flip it so the frame's normal is the outward one. The in-plane
    // basis is rebuilt below, so only the normal needs carrying over.
    double nx = plane_normal.X();
    double ny = plane_normal.Y();
    double nz = plane_normal.Z();
    if (face.Orientation() == TopAbs_REVERSED) {
      nx = -nx;
      ny = -ny;
      nz = -nz;
    }
    if (!normalise(nx, ny, nz)) {
      return std::nullopt;
    }

    // Roll reference: world up, except on a horizontal face where that
    // projection vanishes.
    double ref_x = 0.0;
    double ref_y = 0.0;
    double ref_z = 1.0;
    if (std::abs(nz) > kHorizontalNormalDot) {
      ref_x = 0.0;
      ref_y = 1.0;
      ref_z = 0.0;
    }

    const double ref_dot = ref_x * nx + ref_y * ny + ref_z * nz;
    double y_x = ref_x - ref_dot * nx;
    double y_y = ref_y - ref_dot * ny;
    double y_z = ref_z - ref_dot * nz;
    if (!normalise(y_x, y_y, y_z)) {
      return std::nullopt;
    }

    // x = y x n keeps (x, y, n) right-handed.
    double x_x = y_y * nz - y_z * ny;
    double x_y = y_z * nx - y_x * nz;
    double x_z = y_x * ny - y_y * nx;
    if (!normalise(x_x, x_y, x_z)) {
      return std::nullopt;
    }

    return PlaneFrame{
        .origin_x = origin.X(),
        .origin_y = origin.Y(),
        .origin_z = origin.Z(),
        .x_axis_x = x_x,
        .x_axis_y = x_y,
        .x_axis_z = x_z,
        .y_axis_x = y_x,
        .y_axis_y = y_y,
        .y_axis_z = y_z,
        .normal_x = nx,
        .normal_y = ny,
        .normal_z = nz,
    };
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace polysmith::core
