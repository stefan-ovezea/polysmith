#include "core/drawing/drawing_projection.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <HLRAlgo_EdgeStatus.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_Curve.hxx>
#include <HLRBRep_Data.hxx>
#include <HLRBRep_EdgeData.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

namespace polysmith::core {

namespace {

std::array<double, 2> point2d(const gp_Pnt& p) {
  return {p.X(), p.Y()};
}

std::array<double, 3> point3d(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}

std::array<double, 3> dir3d(const gp_Dir& d) {
  return {d.X(), d.Y(), d.Z()};
}

const char* curve_kind_name(const GeomAbs_CurveType type) {
  switch (type) {
    case GeomAbs_Line: return "line";
    case GeomAbs_Circle: return "circle";
    case GeomAbs_Ellipse: return "ellipse";
    case GeomAbs_BSplineCurve: return "bspline";
    default: return "other";
  }
}

// 1e-9: the status-interval gap threshold from the P0 spike.
constexpr double kIntervalGap = 1e-9;

// ── Deterministic ordering (the golden-file foundation) ──────────

int line_class_rank(const std::string& cls) {
  if (cls == "visible") return 0;
  if (cls == "hidden") return 1;
  return 2;
}

double quant(double v) { return std::round(v * 1e6) / 1e6; }

std::string record_key(const ProjectedEdgeRecord& rec) {
  std::ostringstream os;
  os << std::fixed;
  os.precision(6);
  os << line_class_rank(rec.line_class) << "|" << rec.curve_class << "|"
     << rec.curve_kind << "|";
  if (rec.curve_kind == "line") {
    // Lines: sorted quantised endpoints are order-independent.
    double x0 = quant(rec.p_start[0]);
    double y0 = quant(rec.p_start[1]);
    double x1 = quant(rec.p_end[0]);
    double y1 = quant(rec.p_end[1]);
    if (x0 > x1 || (x0 == x1 && y0 > y1)) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    os << x0 << "," << y0 << "|" << x1 << "," << y1;
  } else {
    // Curved records: start | mid | end (no mid point is stored on
    // the record — re-derive from the parameter range mid-point).
    os << quant(rec.p_start[0]) << "," << quant(rec.p_start[1]) << "|"
       << quant(rec.p_end[0]) << "," << quant(rec.p_end[1]);
  }
  return os.str();
}

/// The source identity for stable sorting: body id + edge index.
/// An empty body id means the witness is unresolved (HLR-derived
/// edges that match no body geometry — chord segments etc.).
std::string source_identity(const ProjectedEdgeRecord& rec) {
  const auto* witness = std::get_if<SourceEdgeWitness>(&rec.source);
  if (witness == nullptr) {
    return "outline";
  }
  if (witness->body_id.empty()) {
    return "unresolved";
  }
  return witness->body_id + "#" + std::to_string(witness->src_edge_index);
}

// Curve-geometry equality within tolerance — matches HLR's split
// copies of an edge against the original body edges (a split circle
// arc shares the original circle's center/axis/radius, a split line
// lies on the original line).
constexpr double kMatchTolerance = 1e-7;

bool same_curve_geometry(const BRepAdaptor_Curve& a,
                         const BRepAdaptor_Curve& b) {
  if (a.GetType() != b.GetType()) {
    return false;
  }
  switch (a.GetType()) {
    case GeomAbs_Line: {
      const gp_Lin la = a.Line();
      const gp_Lin lb = b.Line();
      if (std::abs(la.Direction().Dot(lb.Direction())) <
          1.0 - kMatchTolerance) {
        return false;
      }
      // Perpendicular distance from lb's location to la (a split
      // line's location is a point ON the original line).
      const gp_Vec offset(lb.Location(), la.Location());
      const gp_Vec cross =
          offset.Crossed(gp_Vec(la.Direction()));
      return cross.Magnitude() < kMatchTolerance;
    }
    case GeomAbs_Circle: {
      const gp_Circ ca = a.Circle();
      const gp_Circ cb = b.Circle();
      return ca.Location().Distance(cb.Location()) < kMatchTolerance &&
             std::abs(ca.Radius() - cb.Radius()) < kMatchTolerance &&
             std::abs(ca.Axis().Direction().Dot(cb.Axis().Direction())) >
                 1.0 - kMatchTolerance;
    }
    case GeomAbs_Ellipse: {
      const gp_Elips ea = a.Ellipse();
      const gp_Elips eb = b.Ellipse();
      return ea.Location().Distance(eb.Location()) < kMatchTolerance &&
             std::abs(ea.MajorRadius() - eb.MajorRadius()) <
                 kMatchTolerance &&
             std::abs(ea.MinorRadius() - eb.MinorRadius()) <
                 kMatchTolerance &&
             std::abs(ea.Axis().Direction().Dot(eb.Axis().Direction())) >
                 1.0 - kMatchTolerance;
    }
    default:
      return false;  // splines are not matched — honest "unresolved"
  }
}

}  // namespace

// ── Source edge witness ───────────────────────────────────────────

int match_body_edge(const TopoDS_Shape& body_shape,
                    const TopoDS_Edge& mapped_edge) {
  if (body_shape.IsNull() || mapped_edge.IsNull()) {
    return -1;
  }
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  BRepAdaptor_Curve mapped_curve(mapped_edge);

  for (int i = 1; i <= edge_map.Extent(); ++i) {
    const TopoDS_Edge candidate = TopoDS::Edge(edge_map(i));
    if (candidate.IsSame(mapped_edge)) {
      return i - 1;  // 0-based, the capture_edge_reference convention
    }
    BRepAdaptor_Curve candidate_curve(candidate);
    if (same_curve_geometry(mapped_curve, candidate_curve)) {
      return i - 1;
    }
  }
  return -1;
}

std::optional<SourceEdgeWitness> build_source_edge_witness(
    const std::string& body_id, const TopoDS_Shape& body_shape,
    int edge_index) {
  if (body_shape.IsNull() || edge_index < 0) {
    return std::nullopt;
  }
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);
  const int one_based = edge_index + 1;
  if (one_based < 1 || one_based > edge_map.Extent()) {
    return std::nullopt;
  }
  const TopoDS_Edge edge = TopoDS::Edge(edge_map(one_based));
  if (edge.IsNull()) {
    return std::nullopt;
  }

  TopoDS_Vertex first_vertex;
  TopoDS_Vertex last_vertex;
  TopExp::Vertices(edge, first_vertex, last_vertex, /*CumOri=*/true);
  if (first_vertex.IsNull() || last_vertex.IsNull()) {
    return std::nullopt;
  }
  const gp_Pnt start = BRep_Tool::Pnt(first_vertex);
  const gp_Pnt end = BRep_Tool::Pnt(last_vertex);

  SourceEdgeWitness witness;
  witness.body_id = body_id;
  witness.src_edge_index = edge_index;
  witness.start_point = point3d(start);
  witness.end_point = point3d(end);
  const double dx = end.X() - start.X();
  const double dy = end.Y() - start.Y();
  const double dz = end.Z() - start.Z();
  witness.length = std::sqrt(dx * dx + dy * dy + dz * dz);

  BRepAdaptor_Curve curve(edge);
  witness.curve_kind = curve_kind_name(curve.GetType());
  if (curve.GetType() == GeomAbs_Line && witness.length > 1e-9) {
    witness.tangent = {dx / witness.length, dy / witness.length,
                       dz / witness.length};
  }
  // Circle witness for circles AND arcs (arcs keep their endpoints,
  // so the endpoint witness plus center/axis/radius identifies them;
  // full circles have start == end and need the circle witness).
  if (curve.GetType() == GeomAbs_Circle) {
    const gp_Pnt center = curve.Circle().Location();
    const gp_Dir axis = curve.Circle().Axis().Direction();
    witness.center = point3d(center);
    witness.axis = dir3d(axis);
    witness.radius = curve.Circle().Radius();
  }
  // The status intervals of the HLR entry live in the SOURCE edge's
  // parameter space (the P0 spike finding), so the witness's param
  // range is the source curve's own range.
  witness.param_range = {curve.FirstParameter(), curve.LastParameter()};
  return witness;
}

// ── Standard view frames ──────────────────────────────────────────

std::optional<DrawingViewFrame> standard_view_frame(
    const std::string& standard_view) {
  DrawingViewFrame frame;
  frame.origin = {0.0, 0.0, 0.0};
  // See the header table: every frame keeps view-Y = normal ×
  // x_direction = +Z and matches first-angle placement semantics.
  if (standard_view == "front") {
    frame.normal = {1.0, 0.0, 0.0};
    frame.x_direction = {0.0, 1.0, 0.0};
  } else if (standard_view == "right") {
    frame.normal = {0.0, 1.0, 0.0};
    frame.x_direction = {-1.0, 0.0, 0.0};
  } else if (standard_view == "left") {
    frame.normal = {0.0, -1.0, 0.0};
    frame.x_direction = {1.0, 0.0, 0.0};
  } else if (standard_view == "top") {
    frame.normal = {0.0, 0.0, 1.0};
    frame.x_direction = {0.0, 1.0, 0.0};
  } else if (standard_view == "bottom") {
    frame.normal = {0.0, 0.0, -1.0};
    frame.x_direction = {0.0, -1.0, 0.0};
  } else if (standard_view == "back") {
    frame.normal = {-1.0, 0.0, 0.0};
    frame.x_direction = {0.0, -1.0, 0.0};
  } else {
    return std::nullopt;
  }
  return frame;
}

// ── The projection itself ─────────────────────────────────────────

ProjectionResult project(const ProjectionInput& input) {
  ProjectionResult result;
  result.source_revision = input.source_revision;

  const gp_Ax2 frame(
      gp_Pnt(input.frame.origin[0], input.frame.origin[1],
             input.frame.origin[2]),
      gp_Dir(input.frame.normal[0], input.frame.normal[1],
             input.frame.normal[2]),
      gp_Dir(input.frame.x_direction[0], input.frame.x_direction[1],
             input.frame.x_direction[2]));

  occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
  for (const auto& source : input.sources) {
    algo->Add(source.shape, /*nbIso=*/0);
  }
  HLRAlgo_Projector projector(frame);
  algo->Projector(projector);
  algo->Update();
  algo->Hide();

  const occ::handle<HLRBRep_Data> ds = algo->DataStructure();
  auto& edata = ds->EDataArray();  // non-const: Status() is non-const

  for (int i = 1; i <= ds->NbEdges(); ++i) {
    const HLRBRep_Curve& gc = edata(i).Geometry();
    HLRAlgo_EdgeStatus& st = edata(i).Status();
    const TopoDS_Shape& mapped = ds->EdgeMap()(i);
    const HLRBRep_EdgeData& ed = edata(i);

    const bool has_source =
        !mapped.IsNull() && mapped.ShapeType() == TopAbs_EDGE;
    const std::string curve_class = !has_source      ? "outline"
                                    : ed.RgNLine()   ? "seam"
                                    : ed.Rg1Line()   ? "smooth"
                                                     : "sharp";

    // Parts to emit: (line class, [start, end]) in source-parameter
    // space.
    std::vector<std::pair<std::string, std::pair<double, double>>> parts;
    double bounds_start, bounds_end;
    float ts, te;
    st.Bounds(bounds_start, ts, bounds_end, te);
    if (st.AllHidden()) {
      if (input.show_hidden) {
        parts.push_back({"hidden", {bounds_start, bounds_end}});
      }
    } else {
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        parts.push_back({"visible", {s, e}});
      }
      if (input.show_hidden) {
        // Hidden complement (partially hidden edges): gaps between
        // the visible parts, bounded by the edge bounds.  Fully
        // visible edges contribute no hidden part.
        std::vector<std::pair<double, double>> gaps;
        double cursor = bounds_start;
        for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
          double s, e;
          st.VisiblePart(vp, s, ts, e, te);
          if (s - cursor > kIntervalGap) gaps.push_back({cursor, s});
          cursor = e;
        }
        if (bounds_end - cursor > kIntervalGap) {
          gaps.push_back({cursor, bounds_end});
        }
        for (const auto& gap : gaps) {
          parts.push_back({"hidden", gap});
        }
      }
    }

    for (const auto& part : parts) {
      const TopoDS_Edge piece =
          HLRBRep::MakeEdge(gc, part.second.first, part.second.second);
      if (piece.IsNull()) continue;

      ProjectedEdgeRecord rec;
      rec.line_class = part.first;
      rec.curve_class = curve_class;
      if (has_source) {
        const TopoDS_Edge mapped_edge = TopoDS::Edge(mapped);
        // The mapped edge may be an HLR split copy — match it back
        // to the owning body by IsSame, then curve geometry.
        for (const auto& source : input.sources) {
          const int edge_index =
              match_body_edge(source.shape, mapped_edge);
          if (edge_index >= 0) {
            const auto witness = build_source_edge_witness(
                source.body_id, source.shape, edge_index);
            if (witness.has_value()) {
              rec.source = witness.value();
            }
            break;
          }
        }
      }
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        rec.visible_intervals.push_back({s, e});
      }

      BRepAdaptor_Curve out(piece);
      rec.curve_kind = curve_kind_name(out.GetType());
      rec.first_param = out.FirstParameter();
      rec.last_param = out.LastParameter();
      const gp_Pnt q0 = out.Value(rec.first_param);
      const gp_Pnt q1 = out.Value(rec.last_param);
      // Depth edges (parallel to the view direction) project to
      // degenerate, un-drawable pieces with NaN coordinates — filter
      // them (the P0 spike found them landing hidden; the production
      // engine drops what cannot be drawn).
      if (!std::isfinite(q0.X()) || !std::isfinite(q0.Y()) ||
          !std::isfinite(q1.X()) || !std::isfinite(q1.Y()) ||
          q0.Distance(q1) < 1e-9) {
        continue;
      }
      rec.p_start = point2d(q0);
      rec.p_end = point2d(q1);
      result.edges.push_back(std::move(rec));
    }
  }

  // Silhouettes: extracted from OutLineVCompound (the face-wire path —
  // outline curves are not edge entries).  They carry no source edge;
  // the record carries a body-level FaceAttestation (bounding box of
  // the owning body + the view normal; area/sample_points stay
  // unresolved until dimensions on silhouettes land — P6 upgrades
  // this to a real face attestation).
  HLRBRep_HLRToShape to_shape(algo);
  TopoDS_Shape outline_compound = to_shape.OutLineVCompound();
  BRepLib::SameParameter(outline_compound, Precision::PConfusion(), false);
  for (TopExp_Explorer ex(outline_compound, TopAbs_EDGE); ex.More();
       ex.Next()) {
    const TopoDS_Edge& out_edge = TopoDS::Edge(ex.Current());

    ProjectedEdgeRecord rec;
    rec.line_class = "visible";
    rec.curve_class = "outline";
    // Body-level attestation: silhouettes belong to whichever source
    // body was added — with a single source the owner is unambiguous.
    if (input.sources.size() == 1) {
      FaceAttestation attestation;
      Bnd_Box box;
      BRepBndLib::Add(input.sources[0].shape, box);
      if (!box.IsVoid()) {
        box.Get(attestation.bounds.min_x, attestation.bounds.min_y,
                attestation.bounds.min_z, attestation.bounds.max_x,
                attestation.bounds.max_y, attestation.bounds.max_z);
      }
      attestation.normal = input.frame.normal;
      rec.source = attestation;
    } else {
      // Multi-body: the OutLineVCompound gives no face ownership —
      // leave the attestation unresolved rather than guess.
      FaceAttestation attestation;
      attestation.normal = input.frame.normal;
      rec.source = attestation;
    }

    BRepAdaptor_Curve out(out_edge);
    rec.curve_kind = curve_kind_name(out.GetType());
    rec.first_param = out.FirstParameter();
    rec.last_param = out.LastParameter();
    const gp_Pnt q0 = out.Value(rec.first_param);
    const gp_Pnt q1 = out.Value(rec.last_param);
    rec.p_start = point2d(q0);
    rec.p_end = point2d(q1);
    result.edges.push_back(std::move(rec));
  }

  // Deterministic order: line class rank, curve class, curve kind,
  // geometry signature, source identity.  Records that are exact
  // duplicates (same geometry AND same source — the explorer-revisit
  // case) collapse to one.
  std::sort(result.edges.begin(), result.edges.end(),
            [](const ProjectedEdgeRecord& a, const ProjectedEdgeRecord& b) {
              if (line_class_rank(a.line_class) !=
                  line_class_rank(b.line_class)) {
                return line_class_rank(a.line_class) <
                       line_class_rank(b.line_class);
              }
              if (a.curve_class != b.curve_class) {
                return a.curve_class < b.curve_class;
              }
              const std::string ka = record_key(a);
              const std::string kb = record_key(b);
              if (ka != kb) {
                return ka < kb;
              }
              return source_identity(a) < source_identity(b);
            });
  result.edges.erase(
      std::unique(result.edges.begin(), result.edges.end(),
                  [](const ProjectedEdgeRecord& a,
                     const ProjectedEdgeRecord& b) {
                    return record_key(a) == record_key(b) &&
                           source_identity(a) == source_identity(b);
                  }),
      result.edges.end());

  return result;
}

}  // namespace polysmith::core
