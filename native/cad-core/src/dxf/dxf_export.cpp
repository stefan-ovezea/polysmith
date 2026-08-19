#include "dxf/dxf_export.h"

#include "drw_base.h"
#include "drw_entities.h"
#include "drw_header.h"
#include "drw_interface.h"
#include "libdxfrw.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "core/export/export.h"
#include "core/sketch/sketch_geometry_types.h"
#include "core/sketch/sketch_feature_parameters.h"

namespace polysmith::core {
namespace {

class DxfWriteInterface : public DRW_Interface {
 public:
  DxfWriteInterface(dxfRW* writer, double min_x, double min_y, double max_x,
                    double max_y, int insunits, bool has_bounds)
      : writer_(writer),
        min_x_(min_x),
        min_y_(min_y),
        max_x_(max_x),
        max_y_(max_y),
        insunits_(insunits),
        has_bounds_(has_bounds) {}

  void writeHeader(DRW_Header& data) override {
    // $INSUNITS: 0 = unitless, 1 = inches, 4 = millimetres.
    data.addInt("$INSUNITS", insunits_, 70);
    if (has_bounds_) {
      data.addCoord("$EXTMIN", DRW_Coord{min_x_, min_y_, 0.0}, 10);
      data.addCoord("$EXTMAX", DRW_Coord{max_x_, max_y_, 0.0}, 10);
    }
  }

  void writeEntities() override {
    // The DXF writer converts arc angles from radians to degrees
    // internally (×ARAD), matching the read side's radians convention.
    for (const auto& line : sketch_->lines) {
      DRW_Line entity;
      entity.basePoint.x = line.start_x;
      entity.basePoint.y = line.start_y;
      entity.basePoint.z = 0.0;
      entity.secPoint.x = line.end_x;
      entity.secPoint.y = line.end_y;
      entity.secPoint.z = 0.0;
      writer_->writeLine(&entity);
    }
    for (const auto& circle : sketch_->circles) {
      DRW_Circle entity;
      entity.basePoint.x = circle.center_x;
      entity.basePoint.y = circle.center_y;
      entity.basePoint.z = 0.0;
      entity.radious = circle.radius;
      writer_->writeCircle(&entity);
    }
    for (const auto& arc : sketch_->arcs) {
      DRW_Arc entity;
      entity.basePoint.x = arc.center_x;
      entity.basePoint.y = arc.center_y;
      entity.basePoint.z = 0.0;
      entity.radious = arc.radius;
      double start = std::atan2(arc.start_y - arc.center_y,
                                arc.start_x - arc.center_x);
      double end =
          std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
      // DXF arcs sweep counterclockwise from start to end; normalize a
      // clockwise sketch arc and guard the degenerate empty sweep.
      if (!arc.ccw) {
        std::swap(start, end);
      }
      if (std::abs(end - start) < 1e-9) {
        end = start + 2.0 * M_PI;
      }
      entity.staangle = start;
      entity.endangle = end;
      writer_->writeArc(&entity);
    }
    for (const auto& point : sketch_->projected_points) {
      DRW_Point entity;
      entity.basePoint.x = point.x;
      entity.basePoint.y = point.y;
      entity.basePoint.z = 0.0;
      writer_->writePoint(&entity);
    }
  }

  // All other write callbacks no-op: dxfRW::write() emits the
  // mandatory LTYPE/VPORT table entries itself.
  void writeBlocks() override {}
  void writeBlockRecords() override {}
  void writeLTypes() override {}
  void writeLayers() override {}
  void writeTextstyles() override {}
  void writeVports() override {}
  void writeDimstyles() override {}
  void writeAppId() override {}

  // Read-side callbacks: unused by the writer interface, no-ops.
  void addHeader(const DRW_Header*) override {}
  void addLType(const DRW_LType&) override {}
  void addLayer(const DRW_Layer&) override {}
  void addDimStyle(const DRW_Dimstyle&) override {}
  void addVport(const DRW_Vport&) override {}
  void addTextStyle(const DRW_Textstyle&) override {}
  void addAppId(const DRW_AppId&) override {}
  void addBlock(const DRW_Block&) override {}
  void setBlock(const int) override {}
  void endBlock() override {}
  void addPoint(const DRW_Point&) override {}
  void addLine(const DRW_Line&) override {}
  void addRay(const DRW_Ray&) override {}
  void addXline(const DRW_Xline&) override {}
  void addArc(const DRW_Arc&) override {}
  void addCircle(const DRW_Circle&) override {}
  void addEllipse(const DRW_Ellipse&) override {}
  void addLWPolyline(const DRW_LWPolyline&) override {}
  void addPolyline(const DRW_Polyline&) override {}
  void addSpline(const DRW_Spline*) override {}
  void addKnot(const DRW_Entity&) override {}
  void addInsert(const DRW_Insert&) override {}
  void addTrace(const DRW_Trace&) override {}
  void add3dFace(const DRW_3Dface&) override {}
  void addSolid(const DRW_Solid&) override {}
  void addMText(const DRW_MText&) override {}
  void addText(const DRW_Text&) override {}
  void addDimAlign(const DRW_DimAligned*) override {}
  void addDimLinear(const DRW_DimLinear*) override {}
  void addDimRadial(const DRW_DimRadial*) override {}
  void addDimDiametric(const DRW_DimDiametric*) override {}
  void addDimAngular(const DRW_DimAngular*) override {}
  void addDimAngular3P(const DRW_DimAngular3p*) override {}
  void addDimOrdinate(const DRW_DimOrdinate*) override {}
  void addLeader(const DRW_Leader*) override {}
  void addHatch(const DRW_Hatch*) override {}
  void addViewport(const DRW_Viewport&) override {}
  void addImage(const DRW_Image*) override {}
  void linkImage(const DRW_ImageDef*) override {}
  void addComment(const char*) override {}

  void set_sketch(const SketchFeatureParameters* sketch) {
    sketch_ = sketch;
  }

 private:
  dxfRW* writer_;
  const SketchFeatureParameters* sketch_ = nullptr;
  double min_x_;
  double min_y_;
  double max_x_;
  double max_y_;
  int insunits_;
  bool has_bounds_;
};

}  // namespace

ExportResult export_sketch_as_dxf(const SketchFeatureParameters& sketch,
                                  const std::string& units,
                                  const std::string& file_path) {
  double min_x = std::numeric_limits<double>::max();
  double min_y = std::numeric_limits<double>::max();
  double max_x = std::numeric_limits<double>::lowest();
  double max_y = std::numeric_limits<double>::lowest();
  auto grow = [&](double x, double y) {
    min_x = std::min(min_x, x);
    min_y = std::min(min_y, y);
    max_x = std::max(max_x, x);
    max_y = std::max(max_y, y);
  };
  for (const auto& line : sketch.lines) {
    grow(line.start_x, line.start_y);
    grow(line.end_x, line.end_y);
  }
  for (const auto& circle : sketch.circles) {
    // Conservative bounding box: full radius extent around the center.
    grow(circle.center_x - circle.radius, circle.center_y - circle.radius);
    grow(circle.center_x + circle.radius, circle.center_y + circle.radius);
  }
  for (const auto& arc : sketch.arcs) {
    grow(arc.center_x - arc.radius, arc.center_y - arc.radius);
    grow(arc.center_x + arc.radius, arc.center_y + arc.radius);
  }
  for (const auto& point : sketch.projected_points) {
    grow(point.x, point.y);
  }
  const bool has_bounds =
      !(sketch.lines.empty() && sketch.circles.empty() &&
        sketch.arcs.empty() && sketch.projected_points.empty());

  const int insunits = units == "mm" ? 4 : (units == "in" ? 1 : 0);

  dxfRW dxf(file_path.c_str());
  DxfWriteInterface iface(&dxf, min_x, min_y, max_x, max_y, insunits,
                          has_bounds);
  iface.set_sketch(&sketch);
  if (!dxf.write(&iface, DRW::AC1027, /*bin=*/false)) {
    throw std::runtime_error("Cannot write DXF file: " + file_path);
  }

  const int exported_feature_count =
      static_cast<int>(sketch.lines.size() + sketch.circles.size() +
                       sketch.arcs.size() + sketch.projected_points.size());
  return ExportResult{file_path, "dxf", exported_feature_count};
}

}  // namespace polysmith::core
