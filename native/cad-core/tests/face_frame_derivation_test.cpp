// Regression tests for the canonical face-frame derivation.
//
// The bug: a sketch created on a face of an IMPORTED body came out rolled
// by 180 degrees (sketch +X along world -X, +Y along world -Y) while the
// same sketch on a native body was correct. The derived frame was read
// from the underlying `gp_Pln` placement, which is an artifact of the
// exporting system: a plate lying flat in XY can carry a plane located at
// the part centre and rotated 180 degrees about Z. Native bodies happen to
// carry world-aligned planes, so only imports showed the defect.
//
// The frame must therefore depend only on the face's plane and its
// outward normal — never on how the source file spelled that plane.

#include <cmath>
#include <iostream>
#include <optional>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax3.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include "core/geometry/plane_frame_derivation.h"

namespace {

using polysmith::core::canonical_planar_face_frame;
using polysmith::core::PlaneFrame;

constexpr double kTolerance = 1.0e-9;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = kTolerance) {
  return std::abs(a - b) <= tolerance;
}

// Every case must produce an orthonormal, right-handed basis: the wire
// winding and hole orientation downstream depend on
// cross(x_axis, y_axis) matching the normal.
bool check_basis(const PlaneFrame& frame, const char* label) {
  const double x_len = std::sqrt(frame.x_axis_x * frame.x_axis_x +
                                 frame.x_axis_y * frame.x_axis_y +
                                 frame.x_axis_z * frame.x_axis_z);
  const double y_len = std::sqrt(frame.y_axis_x * frame.y_axis_x +
                                 frame.y_axis_y * frame.y_axis_y +
                                 frame.y_axis_z * frame.y_axis_z);
  const double n_len = std::sqrt(frame.normal_x * frame.normal_x +
                                 frame.normal_y * frame.normal_y +
                                 frame.normal_z * frame.normal_z);
  if (!expect(near(x_len, 1.0) && near(y_len, 1.0) && near(n_len, 1.0),
              label)) {
    return false;
  }
  const double xy_dot = frame.x_axis_x * frame.y_axis_x +
                        frame.x_axis_y * frame.y_axis_y +
                        frame.x_axis_z * frame.y_axis_z;
  if (!expect(near(xy_dot, 0.0), label)) {
    return false;
  }

  // x = y x n, so (x, y, n) is right-handed: x x y == n.
  const double rx = frame.x_axis_y * frame.y_axis_z -
                    frame.x_axis_z * frame.y_axis_y;
  const double ry = frame.x_axis_z * frame.y_axis_x -
                    frame.x_axis_x * frame.y_axis_z;
  const double rz = frame.x_axis_x * frame.y_axis_y -
                    frame.x_axis_y * frame.y_axis_x;
  return expect(near(rx, frame.normal_x) && near(ry, frame.normal_y) &&
                    near(rz, frame.normal_z),
                label);
}

// Build a planar face from an explicit plane placement, the way a STEP
// file can spell a flat plate: a location in the part and a rotation.
TopoDS_Face make_plane_face(const gp_Pnt& location,
                            const gp_Dir& normal,
                            const gp_Dir& x_direction) {
  const gp_Ax3 axes(location, normal, x_direction);
  BRepBuilderAPI_MakeFace builder(gp_Pln(axes), -20.0, 20.0, -20.0, 20.0);
  return builder.Face();
}

bool expect_canonical_top_frame(const std::optional<PlaneFrame>& frame,
                                const char* label) {
  if (!expect(frame.has_value(), label)) {
    return false;
  }
  const PlaneFrame& f = frame.value();
  return expect(near(f.x_axis_x, 1.0) && near(f.x_axis_y, 0.0) &&
                    near(f.x_axis_z, 0.0) && near(f.y_axis_x, 0.0) &&
                    near(f.y_axis_y, 1.0) && near(f.y_axis_z, 0.0) &&
                    near(f.normal_x, 0.0) && near(f.normal_y, 0.0) &&
                    near(f.normal_z, 1.0),
                label) &&
         check_basis(f, label);
}

// THE bug: a plane rotated 180 degrees about Z (as exported by the STEP
// that exposed this) must still yield +X / +Y sketch axes.
bool test_imported_plane_rotation_does_not_roll_the_frame() {
  const TopoDS_Face face =
      make_plane_face(gp_Pnt(35.998804505155, 25.9975, 0.0), gp_Dir(0, 0, 1),
                      gp_Dir(-1, 0, 0));
  const std::optional<PlaneFrame> frame = canonical_planar_face_frame(face);
  if (!expect_canonical_top_frame(
          frame, "imported face: rotated plane still yields world-aligned "
                 "sketch axes")) {
    return false;
  }
  const PlaneFrame& f = frame.value();
  return expect(near(f.origin_x, 35.998804505155) &&
                    near(f.origin_y, 25.9975) && near(f.origin_z, 0.0),
                "imported face: origin stays on the face plane");
}

// The frame must not depend on the source spelling of the plane: the same
// physical plane written with identity axes and with a 180 degree roll
// must produce the identical frame.
bool test_frame_is_independent_of_plane_spelling() {
  const gp_Pnt location(35.998804505155, 25.9975, 0.0);
  const std::optional<PlaneFrame> identity =
      canonical_planar_face_frame(
          make_plane_face(location, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)));
  const std::optional<PlaneFrame> rolled =
      canonical_planar_face_frame(
          make_plane_face(location, gp_Dir(0, 0, 1), gp_Dir(-1, 0, 0)));
  if (!expect(identity.has_value() && rolled.has_value(),
              "equivalent planes: both resolve")) {
    return false;
  }
  const PlaneFrame& a = identity.value();
  const PlaneFrame& b = rolled.value();
  return expect(near(a.origin_x, b.origin_x) && near(a.origin_y, b.origin_y) &&
                    near(a.origin_z, b.origin_z) && near(a.x_axis_x, b.x_axis_x) &&
                    near(a.x_axis_y, b.x_axis_y) && near(a.x_axis_z, b.x_axis_z) &&
                    near(a.y_axis_x, b.y_axis_x) && near(a.y_axis_y, b.y_axis_y) &&
                    near(a.y_axis_z, b.y_axis_z) && near(a.normal_x, b.normal_x) &&
                    near(a.normal_y, b.normal_y) && near(a.normal_z, b.normal_z),
                "equivalent planes: identical frames regardless of spelling");
}

// Native bodies must keep the axes they always had.
bool test_native_box_top_face_stays_canonical() {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(72.0, 52.0, 6.0).Shape();
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
  TopExp::MapShapes(box, TopAbs_FACE, faces);

  bool found_top = false;
  for (int i = 1; i <= faces.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(faces(i));
    const std::optional<PlaneFrame> frame = canonical_planar_face_frame(face);
    if (!frame.has_value()) {
      return expect(false, "box face: planar face resolves");
    }
    if (!near(frame->normal_z, 1.0)) {
      continue;
    }
    found_top = true;
    if (!expect(near(frame->origin_z, 6.0),
                "box top face: frame sits on the face plane")) {
      return false;
    }
    if (!expect_canonical_top_frame(frame, "box top face: canonical axes")) {
      return false;
    }
  }
  return expect(found_top, "box top face: found the +Z face");
}

// A face whose solid lies above it is REVERSED; the sketch normal must
// still point out of the solid and the basis must stay right-handed.
bool test_reversed_face_flips_the_normal_only() {
  TopoDS_Face face = make_plane_face(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1),
                                     gp_Dir(1, 0, 0));
  face.Reverse();

  const std::optional<PlaneFrame> frame = canonical_planar_face_frame(face);
  if (!expect(frame.has_value(), "reversed face: resolves")) {
    return false;
  }
  const PlaneFrame& f = frame.value();
  if (!expect(near(f.normal_z, -1.0), "reversed face: normal points outward")) {
    return false;
  }
  // Horizontal face: +Y supplies the roll, so +X mirrors to keep the
  // basis right-handed.
  return expect(near(f.y_axis_y, 1.0) && near(f.x_axis_x, -1.0) &&
                    near(f.x_axis_z, 0.0),
                "reversed face: right-handed basis mirrored") &&
         check_basis(f, "reversed face: orthonormal right-handed basis");
}

// A tilted face must keep the model's up direction up in the sketch.
bool test_tilted_face_keeps_world_up() {
  const double k = std::sqrt(0.5);
  const TopoDS_Face face =
      make_plane_face(gp_Pnt(0, 0, 0), gp_Dir(k, 0, k), gp_Dir(0, 1, 0));
  const std::optional<PlaneFrame> frame = canonical_planar_face_frame(face);
  if (!expect(frame.has_value(), "tilted face: resolves")) {
    return false;
  }
  const PlaneFrame& f = frame.value();
  if (!expect(near(f.normal_x, k) && near(f.normal_z, k),
              "tilted face: normal follows the face")) {
    return false;
  }
  // +Y is world up projected into the plane: (0,0,1) with the along-normal
  // part removed, i.e. (-k, 0, k).
  if (!expect(near(f.y_axis_x, -k) && near(f.y_axis_y, 0.0) &&
                  near(f.y_axis_z, k),
              "tilted face: sketch +Y is world up projected")) {
    return false;
  }
  return expect(near(f.x_axis_x, 0.0) && near(f.x_axis_y, 1.0) &&
                    near(f.x_axis_z, 0.0),
                "tilted face: sketch +X completes the basis") &&
         check_basis(f, "tilted face: orthonormal right-handed basis");
}

// Vertical faces: up is unambiguous, so both side faces keep +Z as +Y.
bool test_vertical_faces_use_world_up() {
  const TopoDS_Face right = make_plane_face(gp_Pnt(10, 0, 0), gp_Dir(1, 0, 0),
                                            gp_Dir(0, 1, 0));
  const std::optional<PlaneFrame> right_frame = canonical_planar_face_frame(right);
  if (!expect(right_frame.has_value(), "right face: resolves")) {
    return false;
  }
  if (!expect(near(right_frame->x_axis_x, 0.0) &&
                  near(right_frame->x_axis_y, 1.0) &&
                  near(right_frame->y_axis_z, 1.0),
              "right face: +X is world +Y, +Y is world +Z")) {
    return false;
  }
  if (!check_basis(right_frame.value(), "right face: basis")) {
    return false;
  }

  const TopoDS_Face back = make_plane_face(gp_Pnt(0, 10, 0), gp_Dir(0, 1, 0),
                                           gp_Dir(-1, 0, 0));
  const std::optional<PlaneFrame> back_frame = canonical_planar_face_frame(back);
  return expect(back_frame.has_value() && near(back_frame->y_axis_z, 1.0),
                "back face: +Y is still world +Z") &&
         check_basis(back_frame.value(), "back face: basis");
}

// Only planes get a frame; round and null faces do not.
bool test_non_planar_and_null_faces_have_no_frame() {
  if (!expect(!canonical_planar_face_frame(TopoDS_Face()).has_value(),
              "null face: no frame")) {
    return false;
  }

  const TopoDS_Shape cylinder =
      BRepPrimAPI_MakeCylinder(10.0, 20.0).Shape();
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
  TopExp::MapShapes(cylinder, TopAbs_FACE, faces);
  bool found_cylindrical = false;
  for (int i = 1; i <= faces.Extent(); ++i) {
    const TopoDS_Face face = TopoDS::Face(faces(i));
    BRepAdaptor_Surface surface(face, false);
    if (surface.GetType() != GeomAbs_Cylinder) {
      continue;
    }
    found_cylindrical = true;
    if (!expect(!canonical_planar_face_frame(face).has_value(),
                "cylindrical face: no planar frame")) {
      return false;
    }
  }
  return expect(found_cylindrical, "cylinder: found the lateral face");
}

}  // namespace

int main() {
  if (!test_imported_plane_rotation_does_not_roll_the_frame()) return 1;
  if (!test_frame_is_independent_of_plane_spelling()) return 1;
  if (!test_native_box_top_face_stays_canonical()) return 1;
  if (!test_reversed_face_flips_the_normal_only()) return 1;
  if (!test_tilted_face_keeps_world_up()) return 1;
  if (!test_vertical_faces_use_world_up()) return 1;
  if (!test_non_planar_and_null_faces_have_no_frame()) return 1;

  std::cout << "face_frame_derivation_test passed\n";
  return 0;
}
