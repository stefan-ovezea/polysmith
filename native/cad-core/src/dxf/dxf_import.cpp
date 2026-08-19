#include "dxf/dxf_import.h"

#include "drw_base.h"
#include "drw_entities.h"
#include "drw_header.h"
#include "drw_interface.h"
#include "libdxfrw.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace polysmith::core {
namespace {

// Geometry guards mirror the sketch primitive validators: entities
// below these sizes would make add_sketch_* throw, so they are skipped
// and counted instead.
constexpr double kMinSize = 0.001;
constexpr double kBulgeEpsilon = 1e-9;
constexpr double kPlanarEpsilon = 1e-6;
constexpr int kTessellationSegments = 64;
constexpr double kInchesToMm = 25.4;

bool is_planar_z(double z) { return std::abs(z) <= kPlanarEpsilon; }

void record_skipped(DxfImportResult& result, const char* kind) {
  result.skipped_count += 1;
  result.skipped_by_kind[std::string(kind)] += 1;
}

// Converts one polyline segment (possibly bulged) into a line or an
// arc. Bulge b = tan(theta/4) where theta is the included angle;
// b > 0 bulges counterclockwise from p1 to p2.
void append_bulge_segment(DxfImportResult& result, double x1, double y1,
                          double x2, double y2, double bulge) {
  const double dx = x2 - x1;
  const double dy = y2 - y1;
  const double chord = std::hypot(dx, dy);
  if (chord <= kMinSize) {
    record_skipped(result, "degenerate segment");
    return;
  }
  if (std::abs(bulge) <= kBulgeEpsilon) {
    result.lines.push_back(DxfImportLine{x1, y1, x2, y2});
    return;
  }
  const double radius = chord * (1.0 + bulge * bulge) / (4.0 * std::abs(bulge));
  if (radius <= kMinSize) {
    record_skipped(result, "degenerate segment");
    return;
  }
  // Center sits at signed height h from the chord midpoint along the
  // left normal of p1->p2; the sign handles CW bulges and major arcs.
  const double ux = dx / chord;
  const double uy = dy / chord;
  const double height = chord * (1.0 - bulge * bulge) / (4.0 * bulge);
  const double cx = (x1 + x2) / 2.0 - height * uy;
  const double cy = (y1 + y2) / 2.0 + height * ux;
  result.arcs.push_back(
      DxfImportArc{x1, y1, x2, y2, cx, cy, radius, bulge > 0.0});
}

void append_polyline_vertices(DxfImportResult& result,
                              const std::vector<DRW_Vertex2D*>& verts,
                              bool closed, double scale) {
  const int count = static_cast<int>(verts.size());
  if (count < 2) {
    record_skipped(result, "polyline");
    return;
  }
  for (int i = 0; i < count - 1; ++i) {
    append_bulge_segment(result, verts[i]->x * scale, verts[i]->y * scale,
                         verts[i + 1]->x * scale, verts[i + 1]->y * scale,
                         verts[i]->bulge);
  }
  if (closed) {
    const auto* first = verts.front();
    const auto* last = verts.back();
    // Skip a degenerate closing segment (last vertex on top of first).
    if (std::hypot(last->x - first->x, last->y - first->y) > kMinSize) {
      append_bulge_segment(result, last->x * scale, last->y * scale,
                           first->x * scale, first->y * scale, last->bulge);
    }
  }
}

// Standard de Boor evaluation (The NURBS Book, algorithm A2.2) of one
// axis of a B-spline at parameter t. Returns false when the knot vector
// is degenerate at t (repeated knots) — the segment collapses to the
// previous point in that case.
bool de_boor_value(int degree, const std::vector<double>& knots,
                   const std::vector<double>& values, double t,
                   double& out) {
  const int n = static_cast<int>(values.size());
  if (static_cast<int>(knots.size()) != n + degree + 1) {
    return false;
  }
  int span = degree;
  while (span + 1 < n && t >= knots[span + 1]) {
    ++span;
  }
  if (span >= n) {
    span = n - 1;
  }
  std::vector<double> d(degree + 1);
  for (int j = 0; j <= degree; ++j) {
    d[j] = values[span - degree + j];
  }
  for (int r = 1; r <= degree; ++r) {
    for (int j = degree; j >= r; --j) {
      const double denominator =
          knots[span + 1 + j - r] - knots[span - degree + j];
      const double alpha =
          denominator <= kPlanarEpsilon ? 0.0 : (t - knots[span - degree + j]) / denominator;
      d[j] = (1.0 - alpha) * d[j - 1] + alpha * d[j];
    }
  }
  out = d[degree];
  return true;
}

// Tessellates a spline into `kTessellationSegments` straight segments.
// Returns false (nothing emitted) when the data is unusable.
bool append_spline_segments(DxfImportResult& result, const DRW_Spline& spline,
                            double scale) {
  const int degree = spline.degree;
  const int ncontrol = static_cast<int>(spline.controllist.size());
  const std::vector<double>& knots = spline.knotslist;
  if (degree < 1 || ncontrol < degree + 1 ||
      static_cast<int>(knots.size()) != ncontrol + degree + 1) {
    return false;
  }
  // Only planar splines (normal along +-z, or unspecified) make sense
  // for a 2D sketch.
  const double nz = std::abs(spline.normalVec.z);
  const double nlen = std::hypot(spline.normalVec.x, spline.normalVec.y,
                                 spline.normalVec.z);
  if (nlen > kPlanarEpsilon && std::abs(nz - nlen) > kPlanarEpsilon) {
    return false;
  }
  std::vector<double> xs(ncontrol);
  std::vector<double> ys(ncontrol);
  for (int i = 0; i < ncontrol; ++i) {
    xs[i] = spline.controllist[i]->x * scale;
    ys[i] = spline.controllist[i]->y * scale;
  }
  // Valid parameter range is [knots[degree], knots[ncontrol]].
  const double t0 = knots[degree];
  const double t1 = knots[ncontrol];
  if (!(t1 > t0)) {
    return false;
  }
  double px = 0.0;
  double py = 0.0;
  for (int i = 0; i <= kTessellationSegments; ++i) {
    const double t = t0 + (t1 - t0) * static_cast<double>(i) /
                               kTessellationSegments;
    double x = 0.0;
    double y = 0.0;
    if (!de_boor_value(degree, knots, xs, t, x) ||
        !de_boor_value(degree, knots, ys, t, y)) {
      // Degenerate knot span: reuse the previous point rather than
      // emitting a NaN segment.
      x = px;
      y = py;
    }
    if (i > 0) {
      if (std::hypot(x - px, y - py) > kMinSize) {
        result.lines.push_back(DxfImportLine{px, py, x, y});
      }
    }
    px = x;
    py = y;
  }
  return true;
}

void append_ellipse_segments(DxfImportResult& result,
                             const DRW_Ellipse& ellipse, double scale) {
  // secPoint is the major-axis endpoint vector relative to the center
  // (libdxfrw convention — see DRW_Ellipse::toPolyline).
  const double mx = ellipse.secPoint.x * scale;
  const double my = ellipse.secPoint.y * scale;
  const double rad_major = std::hypot(mx, my);
  const double rad_minor = rad_major * ellipse.ratio;
  if (rad_major <= kMinSize || rad_minor <= kMinSize) {
    record_skipped(result, "ELLIPSE");
    return;
  }
  if (std::abs(ellipse.ratio - 1.0) <= kPlanarEpsilon) {
    result.circles.push_back(DxfImportCircle{ellipse.basePoint.x * scale,
                                             ellipse.basePoint.y * scale,
                                             rad_major});
    return;
  }
  const bool full =
      std::abs(ellipse.endparam - ellipse.staparam) < kBulgeEpsilon ||
      std::abs(std::abs(ellipse.endparam - ellipse.staparam) - 2.0 * M_PI) <
          kBulgeEpsilon;
  const double t0 = ellipse.staparam;
  const double t1 = full ? ellipse.staparam + 2.0 * M_PI : ellipse.endparam;
  // Parameterization: p(t) = center + cos(t)*major + sin(t)*ratio*rot90(major),
  // where rot90 rotates the major vector a quarter turn counterclockwise.
  const double rot_x = -my;
  const double rot_y = mx;
  double px = 0.0;
  double py = 0.0;
  for (int i = 0; i <= kTessellationSegments; ++i) {
    const double t =
        t0 + (t1 - t0) * static_cast<double>(i) / kTessellationSegments;
    const double px_now = ellipse.basePoint.x * scale + std::cos(t) * mx +
                          std::sin(t) * ellipse.ratio * rot_x;
    const double py_now = ellipse.basePoint.y * scale + std::cos(t) * my +
                          std::sin(t) * ellipse.ratio * rot_y;
    if (i > 0 && std::hypot(px_now - px, py_now - py) > kMinSize) {
      result.lines.push_back(DxfImportLine{px, py, px_now, py_now});
    }
    px = px_now;
    py = py_now;
  }
  result.warnings.push_back("ellipse approximated as " +
                            std::to_string(kTessellationSegments) +
                            " straight segments");
}

class DxfReadInterface : public DRW_Interface {
 public:
  explicit DxfReadInterface(DxfImportResult& result) : result_(result) {}

  void addHeader(const DRW_Header* data) override {
    // The header vars map is keyed WITH the '$' prefix (group 9 raw).
    // Use find() directly — DRW_Header::getInt deletes the variant.
    const auto it = data->vars.find("$INSUNITS");
    if (it != data->vars.end() && it->second->type() == DRW_Variant::INTEGER &&
        it->second->content.i == 1) {
      scale_ = kInchesToMm;
      result_.units_scaled = true;
    }
  }

  void addPoint(const DRW_Point& data) override {
    if (in_block_) {
      return;
    }
    if (!is_planar_z(data.basePoint.z)) {
      record_skipped(result_, "3D POINT");
      return;
    }
    result_.points.push_back(DxfImportPoint{data.basePoint.x * scale_,
                                            data.basePoint.y * scale_});
  }

  void addLine(const DRW_Line& data) override {
    if (in_block_) {
      return;
    }
    if (!is_planar_z(data.basePoint.z) || !is_planar_z(data.secPoint.z)) {
      record_skipped(result_, "3D LINE");
      return;
    }
    const double x1 = data.basePoint.x * scale_;
    const double y1 = data.basePoint.y * scale_;
    const double x2 = data.secPoint.x * scale_;
    const double y2 = data.secPoint.y * scale_;
    if (std::hypot(x2 - x1, y2 - y1) <= kMinSize) {
      record_skipped(result_, "degenerate line");
      return;
    }
    result_.lines.push_back(DxfImportLine{x1, y1, x2, y2});
  }

  void addCircle(const DRW_Circle& data) override {
    if (in_block_) {
      return;
    }
    const double radius = data.radious * scale_;
    if (radius <= kMinSize) {
      record_skipped(result_, "degenerate circle");
      return;
    }
    result_.circles.push_back(DxfImportCircle{data.basePoint.x * scale_,
                                              data.basePoint.y * scale_,
                                              radius});
  }

  void addArc(const DRW_Arc& data) override {
    if (in_block_) {
      return;
    }
    const double radius = data.radious * scale_;
    if (radius <= kMinSize) {
      record_skipped(result_, "degenerate arc");
      return;
    }
    // libdxfrw stores arc angles in radians (group codes 50/51).
    const double start = data.staangle;
    const double end = data.endangle;
    // DXF convention: equal start/end angles (mod 2π) = full circle.
    if (std::abs(end - start) < kBulgeEpsilon ||
        std::abs(std::abs(end - start) - 2.0 * M_PI) < kBulgeEpsilon) {
      result_.circles.push_back(DxfImportCircle{data.basePoint.x * scale_,
                                                data.basePoint.y * scale_,
                                                radius});
      return;
    }
    // DXF arcs always sweep counterclockwise from start to end.
    result_.arcs.push_back(DxfImportArc{
        data.basePoint.x * scale_ + radius * std::cos(start),
        data.basePoint.y * scale_ + radius * std::sin(start),
        data.basePoint.x * scale_ + radius * std::cos(end),
        data.basePoint.y * scale_ + radius * std::sin(end),
        data.basePoint.x * scale_,
        data.basePoint.y * scale_,
        radius,
        /*ccw=*/true,
    });
  }

  void addLWPolyline(const DRW_LWPolyline& data) override {
    if (in_block_) {
      return;
    }
    // Non-planar (elevation or a tilted extrusion) polylines can't be
    // represented in a 2D sketch — skip and count them.
    if (!is_planar_z(data.elevation) ||
        std::abs(data.extPoint.x) > kPlanarEpsilon ||
        std::abs(data.extPoint.y) > kPlanarEpsilon ||
        std::abs(std::abs(data.extPoint.z) - 1.0) > kPlanarEpsilon) {
      record_skipped(result_, "non-planar LWPOLYLINE");
      return;
    }
    append_polyline_vertices(result_, data.vertlist, (data.flags & 0x1) != 0,
                             scale_);
  }

  void addPolyline(const DRW_Polyline& data) override {
    if (in_block_) {
      return;
    }
    if (std::abs(data.extPoint.x) > kPlanarEpsilon ||
        std::abs(data.extPoint.y) > kPlanarEpsilon ||
        std::abs(std::abs(data.extPoint.z) - 1.0) > kPlanarEpsilon) {
      record_skipped(result_, "non-planar POLYLINE");
      return;
    }
    // Legacy polylines carry vertex z; any 3D vertex makes the whole
    // polyline non-planar.
    std::vector<DRW_Vertex2D> planar_verts;
    planar_verts.reserve(data.vertlist.size());
    for (const auto* vertex : data.vertlist) {
      if (!is_planar_z(vertex->basePoint.z)) {
        record_skipped(result_, "non-planar POLYLINE");
        return;
      }
      DRW_Vertex2D v;
      v.x = vertex->basePoint.x;
      v.y = vertex->basePoint.y;
      v.bulge = vertex->bulge;
      planar_verts.push_back(v);
    }
    std::vector<DRW_Vertex2D*> planar_ptrs;
    planar_ptrs.reserve(planar_verts.size());
    for (auto& v : planar_verts) {
      planar_ptrs.push_back(&v);
    }
    append_polyline_vertices(result_, planar_ptrs, (data.flags & 0x1) != 0,
                             scale_);
  }

  void addSpline(const DRW_Spline* data) override {
    if (in_block_) {
      return;
    }
    if (!append_spline_segments(result_, *data, scale_)) {
      record_skipped(result_, "SPLINE");
      return;
    }
    result_.warnings.push_back("spline approximated as " +
                               std::to_string(kTessellationSegments) +
                               " straight segments");
  }

  void addEllipse(const DRW_Ellipse& data) override {
    if (in_block_) {
      return;
    }
    append_ellipse_segments(result_, data, scale_);
  }

  void addBlock(const DRW_Block&) override {}
  void setBlock(const int) override { in_block_ = true; }
  void endBlock() override { in_block_ = false; }
  void addInsert(const DRW_Insert&) override {
    if (!in_block_) {
      // INSERT references a block definition; block internals are not
      // imported (they would land at block-local coordinates).
      record_skipped(result_, "INSERT");
    }
  }
  void addText(const DRW_Text&) override { record_skipped(result_, "TEXT"); }
  void addMText(const DRW_MText&) override { record_skipped(result_, "MTEXT"); }
  void addHatch(const DRW_Hatch*) override { record_skipped(result_, "HATCH"); }
  void addTrace(const DRW_Trace&) override { record_skipped(result_, "TRACE"); }
  void add3dFace(const DRW_3Dface&) override {
    record_skipped(result_, "3DFACE");
  }
  void addSolid(const DRW_Solid&) override { record_skipped(result_, "SOLID"); }
  void addRay(const DRW_Ray&) override { record_skipped(result_, "RAY"); }
  void addXline(const DRW_Xline&) override { record_skipped(result_, "XLINE"); }
  void addViewport(const DRW_Viewport&) override {
    record_skipped(result_, "VIEWPORT");
  }
  void addImage(const DRW_Image*) override { record_skipped(result_, "IMAGE"); }
  void linkImage(const DRW_ImageDef*) override {}
  void addLeader(const DRW_Leader*) override {
    record_skipped(result_, "LEADER");
  }
  void addDimAlign(const DRW_DimAligned*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addDimLinear(const DRW_DimLinear*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addDimRadial(const DRW_DimRadial*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addDimDiametric(const DRW_DimDiametric*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addDimAngular(const DRW_DimAngular*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addDimAngular3P(const DRW_DimAngular3p*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addDimOrdinate(const DRW_DimOrdinate*) override {
    record_skipped(result_, "DIMENSION");
  }
  void addLType(const DRW_LType&) override {}
  void addLayer(const DRW_Layer&) override {}
  void addDimStyle(const DRW_Dimstyle&) override {}
  void addVport(const DRW_Vport&) override {}
  void addTextStyle(const DRW_Textstyle&) override {}
  void addAppId(const DRW_AppId&) override {}
  void addKnot(const DRW_Entity&) override {}
  void addComment(const char*) override {}

  void writeHeader(DRW_Header&) override {}
  void writeBlocks() override {}
  void writeBlockRecords() override {}
  void writeEntities() override {}
  void writeLTypes() override {}
  void writeLayers() override {}
  void writeTextstyles() override {}
  void writeVports() override {}
  void writeDimstyles() override {}
  void writeAppId() override {}

 private:
  DxfImportResult& result_;
  double scale_ = 1.0;
  bool in_block_ = false;
};

}  // namespace

namespace {

// libdxfrw's ASCII readCode() runs atoi() on the first line, so
// arbitrary text parses as a string of group-code 0 and read()
// "succeeds" with zero entities. Reject non-DXF content up-front: an
// ASCII DXF starts with a numeric group code; a binary DXF with the
// fixed 22-byte sentinel.
bool looks_like_dxf(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary);
  char sentinel[22] = {};
  file.read(sentinel, 21);
  if (std::strncmp(sentinel, "AutoCAD Binary DXF\r\n", 20) == 0) {
    return true;
  }
  file.clear();
  file.seekg(0);
  std::string first;
  while (std::getline(file, first)) {
    if (!first.empty() && first.back() == '\r') {
      first.pop_back();
    }
    if (!first.empty()) {
      break;
    }
  }
  if (first.empty()) {
    return false;
  }
  for (char c : first) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

}  // namespace

DxfImportResult import_dxf_geometry(const std::string& file_path) {
  if (!looks_like_dxf(file_path)) {
    throw std::runtime_error("Could not read DXF file: " + file_path);
  }
  DxfImportResult result;
  DxfReadInterface iface(result);
  dxfRW dxf(file_path.c_str());
  if (!dxf.read(&iface, /*ext=*/true)) {
    throw std::runtime_error("Could not read DXF file: " + file_path);
  }
  return result;
}

}  // namespace polysmith::core
