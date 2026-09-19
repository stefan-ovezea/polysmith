#include "core/drawing/export/drawing_export.h"

#include "drw_entities.h"
#include "drw_header.h"
#include "drw_interface.h"
#include "drw_objects.h"
#include "libdxfrw.h"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace polysmith::core {

namespace {

constexpr const char* kTitleBlockName = "POLYSMITH_TITLE_BLOCK";
constexpr double kPi = 3.14159265358979323846;

/// The entity lineweight enum entry for an ISO line width.
DRW_LW_Conv::lineWidth lw_for(double width_mm) {
  if (width_mm >= 0.7) {
    return DRW_LW_Conv::width14;  // 0.70
  }
  if (width_mm >= 0.5) {
    return DRW_LW_Conv::width11;  // 0.50
  }
  return DRW_LW_Conv::width07;    // 0.25 — the ISO thin default
}

/// The layer an entity lands on.  All stream layers are CONTINUOUS:
/// the core flatten already applied the ISO 128-2 dash patterns (with
/// the Annex-A corner rule) — re-dashing would double them.  The
/// HIDDEN/CHAIN linetypes are still DEFINED for user reuse and the P9
/// annotated mode.
const char* layer_for(const SheetPrimitive& p) {
  if (p.purpose == "frame") {
    return "FRAME";
  }
  if (p.purpose == "hatch") {
    return "HATCH";
  }
  if (p.purpose == "cutting_plane") {
    return "CUTTING";
  }
  if (p.purpose == "dimension" || p.purpose == "section_label") {
    return "ANNOTATION";
  }
  if (p.purpose == "view_geometry") {
    return p.line_class == "hidden" ? "HIDDEN" : "VISIBLE";
  }
  return "FURNITURE";  // centring marks, grid refs, title block
}

class DrawingDxfInterface : public DRW_Interface {
 public:
  DrawingDxfInterface(dxfRW* writer, const SheetPrimitiveStream& stream)
      : writer_(writer), stream_(stream) {
    // Split the stream: the title block (lines + projection symbol +
    // field texts) travels as a BLOCK + INSERT; everything else stays
    // direct entities.  Glyph primitives are skipped — the text
    // records below become real DRW_Text entities (a drawing must not
    // carry its text twice).
    for (const auto& p : stream.primitives) {
      if (p.purpose == "text_glyph") {
        continue;
      }
      if (p.purpose == "title_block" || p.purpose == "projection_symbol") {
        block_primitives_.push_back(&p);
      } else {
        direct_primitives_.push_back(&p);
      }
    }
    for (const auto& t : stream.texts) {
      if (t.purpose == "title_block") {
        block_texts_.push_back(&t);
      } else {
        direct_texts_.push_back(&t);
      }
    }
    exported_count_ = static_cast<int>(block_primitives_.size() +
                                       direct_primitives_.size() +
                                       block_texts_.size() +
                                       direct_texts_.size());
  }

  void writeHeader(DRW_Header& data) override {
    data.addInt("$INSUNITS", 4, 70);  // millimetres
    data.addCoord("$EXTMIN", DRW_Coord{0.0, 0.0, 0.0}, 10);
    data.addCoord("$EXTMAX",
                  DRW_Coord{stream_.width_mm, stream_.height_mm, 0.0}, 10);
  }

  // The libdxfrw UB trap: every block name must be REGISTERED here
  // before writeBlock() is called — dxfRW looks the name up in the
  // map writeBlockRecord() seeds.
  void writeBlockRecords() override {
    if (has_block()) {
      writer_->writeBlockRecord(kTitleBlockName);
    }
  }

  void writeBlocks() override {
    if (!has_block()) {
      return;
    }
    DRW_Block block;
    block.name = kTitleBlockName;
    block.basePoint.x = 0.0;
    block.basePoint.y = 0.0;
    block.basePoint.z = 0.0;
    writer_->writeBlock(&block);
    for (const SheetPrimitive* p : block_primitives_) {
      write_primitive(*p);
    }
    for (const SheetText* t : block_texts_) {
      write_text(*t);
    }
  }

  void writeEntities() override {
    for (const SheetPrimitive* p : direct_primitives_) {
      write_primitive(*p);
    }
    for (const SheetText* t : direct_texts_) {
      write_text(*t);
    }
    if (has_block()) {
      DRW_Insert insert;
      insert.name = kTitleBlockName;
      insert.layer = "FURNITURE";
      insert.basePoint.x = 0.0;
      insert.basePoint.y = 0.0;
      insert.basePoint.z = 0.0;
      insert.xscale = 1.0;
      insert.yscale = 1.0;
      insert.zscale = 1.0;
      writer_->writeInsert(&insert);
    }
  }

  // ISO 128-2 patterns at d = 0.25: dashed 12d/3d, chain 24d/3d/dot.
  // Defined for user reuse + the P9 annotated mode (see layer_for()).
  void writeLTypes() override {
    DRW_LType hidden;
    hidden.name = "HIDDEN";
    hidden.desc = "ISO 128-2 dashed (12d/3d)";
    hidden.size = 2;
    hidden.path = {3.0, -0.75};
    hidden.length = 3.75;
    writer_->writeLineType(&hidden);

    DRW_LType chain;
    chain.name = "CHAIN";
    chain.desc = "ISO 128-2 chain (24d/3d/dot)";
    chain.size = 4;
    chain.path = {6.0, -0.75, 0.0, -0.75};
    chain.length = 7.5;
    writer_->writeLineType(&chain);
  }

  void writeLayers() override {
    const struct {
      const char* name;
      DRW_LW_Conv::lineWidth weight;
    } layers[] = {
        {"VISIBLE", DRW_LW_Conv::width11},
        {"HIDDEN", DRW_LW_Conv::width07},
        {"CUTTING", DRW_LW_Conv::width07},
        {"HATCH", DRW_LW_Conv::width07},
        {"ANNOTATION", DRW_LW_Conv::width07},
        {"FURNITURE", DRW_LW_Conv::width07},
        {"FRAME", DRW_LW_Conv::width14},
        {"TEXT", DRW_LW_Conv::width07},
    };
    for (const auto& entry : layers) {
      DRW_Layer layer;
      layer.name = entry.name;
      layer.lineType = "CONTINUOUS";
      layer.lWeight = entry.weight;
      writer_->writeLayer(&layer);
    }
  }

  void writeTextstyles() override {}
  void writeVports() override {}
  void writeDimstyles() override {}
  void writeAppId() override {}

  int exported_count() const { return exported_count_; }

 private:
  bool has_block() const {
    return !block_primitives_.empty() || !block_texts_.empty();
  }

  void stamp(DRW_Entity* entity, const SheetPrimitive& p) {
    entity->layer = layer_for(p);
    entity->lWeight = lw_for(p.style.width_mm);
  }

  void write_primitive(const SheetPrimitive& p) {
    if (p.kind == "line") {
      DRW_Line entity;
      stamp(&entity, p);
      entity.basePoint.x = p.p0[0];
      entity.basePoint.y = p.p0[1];
      entity.basePoint.z = 0.0;
      entity.secPoint.x = p.p1[0];
      entity.secPoint.y = p.p1[1];
      entity.secPoint.z = 0.0;
      writer_->writeLine(&entity);
      return;
    }
    if (p.kind == "circle_arc" && p.center.has_value() &&
        p.radius.has_value()) {
      const double sweep = std::abs(p.end_angle - p.start_angle);
      if (sweep >= 2.0 * kPi - 1e-9) {
        DRW_Circle entity;
        stamp(&entity, p);
        entity.basePoint.x = p.center.value()[0];
        entity.basePoint.y = p.center.value()[1];
        entity.basePoint.z = 0.0;
        entity.radious = p.radius.value();
        writer_->writeCircle(&entity);
      } else {
        // The writer converts arc angles from radians to degrees
        // internally (×ARAD), matching the read side's convention.
        DRW_Arc entity;
        stamp(&entity, p);
        entity.basePoint.x = p.center.value()[0];
        entity.basePoint.y = p.center.value()[1];
        entity.basePoint.z = 0.0;
        entity.radious = p.radius.value();
        entity.staangle = p.start_angle;
        entity.endangle = p.end_angle;
        writer_->writeArc(&entity);
      }
      return;
    }
    if (p.kind == "ellipse_arc" && p.center.has_value() &&
        p.major_dir.has_value() && p.major_radius.has_value() &&
        p.minor_radius.has_value()) {
      DRW_Ellipse entity;
      stamp(&entity, p);
      entity.basePoint.x = p.center.value()[0];
      entity.basePoint.y = p.center.value()[1];
      entity.basePoint.z = 0.0;
      entity.secPoint.x =
          p.center.value()[0] + p.major_dir.value()[0] *
                                    p.major_radius.value();
      entity.secPoint.y =
          p.center.value()[1] + p.major_dir.value()[1] *
                                    p.major_radius.value();
      entity.secPoint.z = 0.0;
      entity.ratio = p.minor_radius.value() / p.major_radius.value();
      entity.staparam = p.start_angle;
      entity.endparam = p.end_angle;
      writer_->writeEllipse(&entity);
      return;
    }
    if (p.kind == "filled_poly") {
      // Dimension/section arrowheads are triangles; anything larger
      // fans from the first vertex into DXF SOLID quads (the SOLID
      // entity takes 4 corners — degenerate triangles repeat one).
      for (size_t i = 1; i + 1 < p.points.size(); ++i) {
        DRW_Solid entity;
        stamp(&entity, p);
        entity.basePoint.x = p.points[0][0];
        entity.basePoint.y = p.points[0][1];
        entity.basePoint.z = 0.0;
        entity.secPoint.x = p.points[i][0];
        entity.secPoint.y = p.points[i][1];
        entity.secPoint.z = 0.0;
        entity.thirdPoint.x = p.points[i + 1][0];
        entity.thirdPoint.y = p.points[i + 1][1];
        entity.thirdPoint.z = 0.0;
        entity.fourPoint = entity.thirdPoint;
        writer_->writeSolid(&entity);
      }
      return;
    }
    // Unknown kind — skipped defensively (the stream vocabulary is
    // fixed by the flatten).
  }

  void write_text(const SheetText& t) {
    DRW_Text entity;
    entity.layer = "TEXT";
    entity.lWeight = DRW_LW_Conv::width07;
    entity.basePoint.x = t.position[0];
    entity.basePoint.y = t.position[1];
    entity.basePoint.z = 0.0;
    entity.height = t.height_mm;
    entity.angle = t.angle_deg;
    entity.text = t.text;
    if (t.h_align == "left") {
      entity.alignH = DRW_Text::HLeft;
    } else if (t.h_align == "right") {
      entity.alignH = DRW_Text::HRight;
    } else {
      entity.alignH = DRW_Text::HCenter;
    }
    entity.alignV = DRW_Text::VMiddle;  // the SheetText anchor is centered
    writer_->writeText(&entity);
  }

  // Read-side callbacks: unused by the writer, no-ops.
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

  dxfRW* writer_;
  const SheetPrimitiveStream& stream_;
  std::vector<const SheetPrimitive*> block_primitives_;
  std::vector<const SheetPrimitive*> direct_primitives_;
  std::vector<const SheetText*> block_texts_;
  std::vector<const SheetText*> direct_texts_;
  int exported_count_ = 0;
};

}  // namespace

ExportResult export_sheet_as_dxf(const SheetPrimitiveStream& stream,
                                 const std::string& file_path) {
  dxfRW dxf(file_path.c_str());
  DrawingDxfInterface iface(&dxf, stream);
  if (!dxf.write(&iface, DRW::AC1027, /*bin=*/false)) {
    throw std::runtime_error("Cannot write DXF file: " + file_path);
  }
  return ExportResult{file_path, "dxf", iface.exported_count()};
}

}  // namespace polysmith::core
