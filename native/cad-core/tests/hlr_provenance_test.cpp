// HLR provenance spike — P0 of the ISO Drawing workbench.
//
// Verifies the design bet behind the drawing workbench: that OCCT's
// public HLR data structure (HLRBRep_InternalAlgo::DataStructure())
// exposes, per entry, the source topology (EdgeMap: index -> source
// TopoDS_Edge), the visibility state (EDataArray: HLRBRep_EdgeData
// with HLRAlgo_EdgeStatus visible intervals), and the projected
// curve (HLRBRep_Curve) — so every projected edge on a drawing sheet
// can carry a model-anchored witness (source edge + visible
// parameter interval) instead of a projection ordinal.
//
// The spike's key finding (encoded here): matching HLRToShape's
// output compounds back to entries is fragile (analytic vs pointwise
// projection disagree ~0.002-0.016 mm; output edges are re-
// parametrized in 2D space).  The robust design is the reverse —
// iterate the EDataArray directly and build each drawing record with
// HLRBRep::MakeEdge(entry curve, visible interval), so provenance is
// born with the record and no matching heuristics exist at all.
//
// The helpers here are spike-quality (test-local); P2 promotes them
// into core/drawing/drawing_projection.{h,cpp}.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
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
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

namespace {

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

// ── Drawing record with provenance ─────────────────────────────────
// The shape the drawing core will emit per projected edge piece (P2).
// Provenance is born with the record: it is built FROM an EDataArray
// entry, never matched back to one.

struct EdgeRecord {
  std::string line_class;   // "visible" | "hidden"
  std::string curve_class;  // "sharp" | "smooth" | "seam" | "outline"
  std::string curve_kind;   // GeomAbs_CurveType name of the OUTPUT edge
  gp_Pnt2d p_start;         // 2D, view space (projection plane)
  gp_Pnt2d p_end;
  gp_Pnt2d p_mid;
  double first_param = 0;   // params of the OUTPUT (2D) curve
  double last_param = 0;
  // Provenance (from the entry the record was built from):
  bool has_source_edge = false;   // EdgeMap entry resolves to a real edge
  TopoDS_Edge source_edge;        // null when has_source_edge is false
                                  // (pure silhouette -> FaceAttestation)
  int source_entry_index = -1;    // EDataArray index (1-based)
  int visible_parts = 0;          // of the SOURCE entry's status
  std::vector<std::pair<double, double>> visible_intervals;  // source-param space
};

// ── HLR runner: direct entry iteration ─────────────────────────────

struct HlrOutput {
  occ::handle<HLRBRep_Algo> algo;  // keeps the data structure alive
  std::vector<EdgeRecord> records;
  std::vector<EdgeRecord> silhouettes;  // from OutLineVCompound — no source
                                        // edge (body/face-attestation bucket)
  int entry_count = 0;
};

// Builds one record per (entry, visible/hidden part).  For every
// part, HLRBRep::MakeEdge constructs the exact 2D curve piece —
// the same construction HLRToShape uses internally — so the record's
// geometry, source edge, and parameter interval are inseparable.
HlrOutput run_hlr(const TopoDS_Shape& shape, const gp_Ax2& frame,
                  bool verbose) {
  HlrOutput result;

  result.algo = new HLRBRep_Algo();
  result.algo->Add(shape, /*nbIso=*/0);
  HLRAlgo_Projector projector(frame);
  result.algo->Projector(projector);
  result.algo->Update();
  result.algo->Hide();

  occ::handle<HLRBRep_Data> ds = result.algo->DataStructure();
  result.entry_count = ds->NbEdges();
  auto& edata = ds->EDataArray();  // non-const: Status() is non-const

  for (int i = 1; i <= ds->NbEdges(); ++i) {
    const HLRBRep_Curve& gc = edata(i).Geometry();
    HLRAlgo_EdgeStatus& st = edata(i).Status();
    const TopoDS_Shape& mapped = ds->EdgeMap()(i);
    const HLRBRep_EdgeData& ed = edata(i);

    const bool has_source =
        !mapped.IsNull() && mapped.ShapeType() == TopAbs_EDGE;
    const std::string curve_class = !has_source     ? "outline"
                                    : ed.RgNLine()  ? "seam"
                                    : ed.Rg1Line()  ? "smooth"
                                                    : "sharp";

    // Parts to emit: (class, [start, end]) in source-parameter space.
    std::vector<std::pair<std::string, std::pair<double, double>>> parts;
    double bounds_start, bounds_end;
    float ts, te;
    st.Bounds(bounds_start, ts, bounds_end, te);
    if (st.AllHidden()) {
      parts.push_back({"hidden", {bounds_start, bounds_end}});
    } else {
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        parts.push_back({"visible", {s, e}});
      }
      // Hidden complement (partially hidden edges): gaps between the
      // visible parts, bounded by the edge bounds.  Fully visible
      // edges contribute no hidden part.
      std::vector<std::pair<double, double>> gaps;
      double cursor = bounds_start;
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        if (s - cursor > 1e-9) gaps.push_back({cursor, s});
        cursor = e;
      }
      if (bounds_end - cursor > 1e-9) gaps.push_back({cursor, bounds_end});
      for (const auto& gap : gaps) {
        parts.push_back({"hidden", gap});
      }
    }

    for (const auto& part : parts) {
      const TopoDS_Edge piece =
          HLRBRep::MakeEdge(gc, part.second.first, part.second.second);
      if (piece.IsNull()) continue;

      EdgeRecord rec;
      rec.line_class = part.first;
      rec.curve_class = curve_class;
      rec.has_source_edge = has_source;
      if (has_source) rec.source_edge = TopoDS::Edge(mapped);
      rec.source_entry_index = i;
      rec.visible_parts = st.NbVisiblePart();
      for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
        double s, e;
        st.VisiblePart(vp, s, ts, e, te);
        rec.visible_intervals.push_back({s, e});
      }

      BRepAdaptor_Curve out(piece);
      switch (out.GetType()) {
        case GeomAbs_Line: rec.curve_kind = "line"; break;
        case GeomAbs_Circle: rec.curve_kind = "circle"; break;
        case GeomAbs_Ellipse: rec.curve_kind = "ellipse"; break;
        case GeomAbs_BSplineCurve: rec.curve_kind = "bspline"; break;
        default: rec.curve_kind = "other"; break;
      }
      rec.first_param = out.FirstParameter();
      rec.last_param = out.LastParameter();
      const gp_Pnt q0 = out.Value(rec.first_param);
      const gp_Pnt q1 = out.Value(rec.last_param);
      const gp_Pnt qm = out.Value((rec.first_param + rec.last_param) / 2.0);
      rec.p_start = gp_Pnt2d(q0.X(), q0.Y());
      rec.p_end = gp_Pnt2d(q1.X(), q1.Y());
      rec.p_mid = gp_Pnt2d(qm.X(), qm.Y());
      result.records.push_back(rec);
    }
  }

  // Silhouettes: extracted from OutLineVCompound (the face-wire path —
  // outline curves are not edge entries).  They carry no source edge;
  // the drawing record schema's FaceAttestation variant covers them.
  HLRBRep_HLRToShape to_shape(result.algo);
  TopoDS_Shape outline_compound = to_shape.OutLineVCompound();
  BRepLib::SameParameter(outline_compound, Precision::PConfusion(), false);
  for (TopExp_Explorer ex(outline_compound, TopAbs_EDGE); ex.More(); ex.Next()) {
    const TopoDS_Edge& out_edge = TopoDS::Edge(ex.Current());
    EdgeRecord rec;
    rec.line_class = "visible";
    rec.curve_class = "outline";
    rec.has_source_edge = false;
    BRepAdaptor_Curve out(out_edge);
    switch (out.GetType()) {
      case GeomAbs_Line: rec.curve_kind = "line"; break;
      case GeomAbs_Circle: rec.curve_kind = "circle"; break;
      case GeomAbs_Ellipse: rec.curve_kind = "ellipse"; break;
      case GeomAbs_BSplineCurve: rec.curve_kind = "bspline"; break;
      default: rec.curve_kind = "other"; break;
    }
    rec.first_param = out.FirstParameter();
    rec.last_param = out.LastParameter();
    const gp_Pnt q0 = out.Value(rec.first_param);
    const gp_Pnt q1 = out.Value(rec.last_param);
    const gp_Pnt qm = out.Value((rec.first_param + rec.last_param) / 2.0);
    rec.p_start = gp_Pnt2d(q0.X(), q0.Y());
    rec.p_end = gp_Pnt2d(q1.X(), q1.Y());
    rec.p_mid = gp_Pnt2d(qm.X(), qm.Y());
    result.silhouettes.push_back(rec);
  }

  if (verbose) {
    std::cout << "HLR data structure: " << ds->NbEdges() << " edge entries, "
              << result.records.size() << " drawing records, "
              << result.silhouettes.size() << " silhouettes\n";
    int visible = 0;
    int hidden = 0;
    for (const auto& rec : result.records) {
      if (rec.line_class == "hidden") ++hidden;
      else ++visible;
    }
    std::cout << "  -> " << visible << " visible, " << hidden << " hidden\n";
  }
  return result;
}

// ── Deterministic serialisation (the golden-file foundation) ───────

double quant(double v) { return std::round(v * 1e6) / 1e6; }

int class_rank(const std::string& cls) {
  if (cls == "visible") return 0;
  if (cls == "hidden") return 1;
  return 2;
}

std::string record_key(const EdgeRecord& rec) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(6);
  os << class_rank(rec.line_class) << "|" << rec.curve_class << "|"
     << rec.curve_kind << "|";
  if (rec.curve_kind == "line") {
    // Lines: sorted quantised endpoints are order-independent.
    double x0 = quant(rec.p_start.X());
    double y0 = quant(rec.p_start.Y());
    double x1 = quant(rec.p_end.X());
    double y1 = quant(rec.p_end.Y());
    if (x0 > x1 || (x0 == x1 && y0 > y1)) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    os << x0 << "," << y0 << "|" << x1 << "," << y1;
  } else {
    os << quant(rec.p_start.X()) << "," << quant(rec.p_start.Y()) << "|"
       << quant(rec.p_mid.X()) << "," << quant(rec.p_mid.Y()) << "|"
       << quant(rec.p_end.X()) << "," << quant(rec.p_end.Y());
  }
  return os.str();
}

std::string canonical_records(const HlrOutput& output) {
  std::vector<std::string> keys;
  for (const auto& rec : output.records) keys.push_back(record_key(rec));
  std::sort(keys.begin(), keys.end());
  std::ostringstream os;
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i) os << "\n";
    os << keys[i];
  }
  return os.str();
}

// ── Source-edge coordinate helpers ─────────────────────────────────

// Collects every source edge of `shape` that is a straight line with
// endpoints (a,b) or (b,a) within tol.  TopExp_Explorer revisits
// shared sub-shapes (each box edge is reached through both of its
// faces), so matches are de-duplicated with IsSame.
std::vector<TopoDS_Edge> find_line_edges(const TopoDS_Shape& shape,
                                         const gp_Pnt& a, const gp_Pnt& b,
                                         double tol) {
  std::vector<TopoDS_Edge> out;
  for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(ex.Current());
    BRepAdaptor_Curve c(edge);
    if (c.GetType() != GeomAbs_Line) continue;
    const gp_Pnt p0 = c.Value(c.FirstParameter());
    const gp_Pnt p1 = c.Value(c.LastParameter());
    const bool forward = p0.Distance(a) < tol && p1.Distance(b) < tol;
    const bool reverse = p0.Distance(b) < tol && p1.Distance(a) < tol;
    if (!forward && !reverse) continue;
    bool seen = false;
    for (const auto& candidate : out) {
      if (candidate.IsSame(edge)) {
        seen = true;
        break;
      }
    }
    if (!seen) out.push_back(edge);
  }
  return out;
}

bool edge_in_set(const TopoDS_Edge& edge,
                 const std::vector<TopoDS_Edge>& set) {
  for (const auto& candidate : set) {
    if (candidate.IsSame(edge)) return true;
  }
  return false;
}

// ── Test 1: box, front projection — provenance resolves ─────────────

bool test_box_front_projection() {
  // Box 10 x 20 x 30 centred at the origin.  Front view along +X
  // (frame normal = +X, sheet X = +Y): the four edges of the x=+5
  // face are the visible sharp edges; the x=-5 face edges are hidden.
  TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(-5, -10, -15),
                                         gp_Pnt(5, 10, 15)).Shape();
  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
  HlrOutput out = run_hlr(box, frame, /*verbose=*/true);

  if (!expect(!out.records.empty(), "box front: drawing records exist")) {
    return false;
  }

  // Collect the visible/hidden records whose source is a front-face
  // or back-face edge.
  const gp_Pnt front[4] = {
      gp_Pnt(5, -10, -15), gp_Pnt(5, 10, -15),
      gp_Pnt(5, 10, 15), gp_Pnt(5, -10, 15),
  };
  const gp_Pnt back[4] = {
      gp_Pnt(-5, -10, -15), gp_Pnt(-5, 10, -15),
      gp_Pnt(-5, 10, 15), gp_Pnt(-5, -10, 15),
  };
  std::vector<TopoDS_Edge> front_edges;
  std::vector<TopoDS_Edge> back_edges;
  for (int i = 0; i < 4; ++i) {
    const auto fe = find_line_edges(box, front[i], front[(i + 1) % 4], 1e-6);
    const auto be = find_line_edges(box, back[i], back[(i + 1) % 4], 1e-6);
    front_edges.insert(front_edges.end(), fe.begin(), fe.end());
    back_edges.insert(back_edges.end(), be.begin(), be.end());
  }
  if (!expect(front_edges.size() == 4 && back_edges.size() == 4,
              "box front: 4 front-face and 4 back-face edges")) {
    return false;
  }

  // Full-set assertion: every front-face edge produces exactly one
  // VISIBLE record; every back-face edge produces exactly one HIDDEN
  // record; the 4 depth edges (parallel to the view direction) are
  // degenerate in projection and land in the hidden bucket.
  int front_visible = 0;
  int back_hidden = 0;
  int depth_hidden = 0;
  for (const auto& rec : out.records) {
    if (!rec.has_source_edge) {
      if (!expect(false, "box front: box has no silhouette records")) {
        return false;
      }
    }
    if (edge_in_set(rec.source_edge, front_edges)) {
      ++front_visible;
      if (!expect(rec.line_class == "visible" && rec.curve_class == "sharp",
                  "box front: front-face edge record is visible sharp")) {
        return false;
      }
    } else if (edge_in_set(rec.source_edge, back_edges)) {
      ++back_hidden;
      if (!expect(rec.line_class == "hidden",
                  "box front: back-face edge record is hidden")) {
        return false;
      }
    } else {
      // The only remaining source edges are the 4 depth edges.
      ++depth_hidden;
      if (!expect(rec.line_class == "hidden",
                  "box front: depth-edge record is hidden")) {
        return false;
      }
    }
  }
  if (!expect(front_visible == 4,
              "box front: all 4 front edges visible")) {
    return false;
  }
  if (!expect(back_hidden == 4,
              "box front: all 4 back edges hidden")) {
    return false;
  }
  if (!expect(depth_hidden == 4,
              "box front: all 4 depth edges hidden")) {
    return false;
  }
  // Every visible record's source entry must carry a visible part.
  for (const auto& rec : out.records) {
    if (rec.line_class == "visible") {
      if (!expect(rec.visible_parts >= 1,
                  "box front: visible records have visible parts")) {
        return false;
      }
    }
  }
  return true;
}

// ── Test 2: silhouette provenance (cylinder, off-axis) ──────────────

bool test_silhouette_provenance() {
  // Bare cylinder viewed off-axis: the smooth side surface generates
  // silhouette outlines that have NO source edge — the drawing record
  // schema carries a FaceAttestation variant for exactly this case.
  TopoDS_Shape cylinder =
      BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                               /*radius=*/10.0, /*height=*/30.0).Shape();
  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(1, 0.4, 0.2), gp_Dir(0, 1, 0));
  HlrOutput out = run_hlr(cylinder, frame, /*verbose=*/true);

  // Silhouettes (OutLineVCompound) exist off-axis and carry NO source
  // edge — the FaceAttestation bucket of the drawing record schema.
  if (!expect(out.silhouettes.size() >= 2,
              "silhouette: cylinder off-axis produces silhouette curves")) {
    return false;
  }
  for (const auto& rec : out.silhouettes) {
    if (!expect(!rec.has_source_edge,
                "silhouette: silhouette records carry no source edge")) {
      return false;
    }
  }
  // Edge-derived records: the rim circles (2 source edges) project to
  // ellipse arcs and carry their source edges.
  int with_source = 0;
  for (const auto& rec : out.records) {
    if (rec.has_source_edge) ++with_source;
  }
  if (!expect(with_source >= 2,
              "silhouette: rim arcs carry source edges")) {
    return false;
  }
  // Rim projections must be ellipses (oblique circle projection).
  bool saw_ellipse = false;
  for (const auto& rec : out.records) {
    if (rec.curve_kind == "ellipse") {
      saw_ellipse = true;
      break;
    }
  }
  std::cout << "cylinder off-axis: " << out.records.size() << " edge records, "
            << out.silhouettes.size() << " silhouettes\n";
  return expect(saw_ellipse,
                "silhouette: oblique rim projection is an ellipse");
}

// ── Test 3: visible-interval split on partial occlusion ─────────────

bool test_visible_interval_split() {
  // Slab A (0..100 x 0..10 x 0..30) with a tall box B (0..40 x 20..30
  // x 0..80) in front of it.  Viewed along +Y, B occludes A's front
  // top edge for x in [0, 40]; x in [40, 100] stays visible.  The
  // source edge must therefore carry ONE visible interval covering
  // exactly the unoccluded part.
  TopoDS_Shape a =
      BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(100, 10, 30)).Shape();
  TopoDS_Shape b =
      BRepPrimAPI_MakeBox(gp_Pnt(0, 20, 0), gp_Pnt(40, 30, 80)).Shape();
  TopoDS_Shape fused = BRepAlgoAPI_Fuse(a, b).Shape();

  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0), gp_Dir(1, 0, 0));
  HlrOutput out = run_hlr(fused, frame, /*verbose=*/true);

  const gp_Pnt edge_start(0, 10, 30);
  const gp_Pnt edge_end(100, 10, 30);
  const auto source_edges =
      find_line_edges(fused, edge_start, edge_end, 1e-6);
  if (!expect(source_edges.size() == 1,
              "interval: the slab front-top edge exists on the fused body")) {
    return false;
  }

  // The visible record for that source edge: a single visible part.
  const EdgeRecord* hit = nullptr;
  for (const auto& rec : out.records) {
    if (rec.line_class == "visible" && rec.has_source_edge &&
        rec.source_edge.IsSame(source_edges[0])) {
      hit = &rec;
      break;
    }
  }
  if (!expect(hit != nullptr,
              "interval: partially hidden edge emits a visible record")) {
    return false;
  }
  if (!expect(hit->visible_parts == 1,
              "interval: exactly one visible part for the slab edge")) {
    return false;
  }
  const auto& iv = hit->visible_intervals[0];

  // Map the interval endpoints back to 3D with the data structure's
  // own geometry (the P2 engine writes witness intervals exactly this
  // way) and assert the visible segment is x in [40,100].  Status
  // intervals live in the entry curve's 3D parameter space, so
  // Value3D takes them directly.
  const occ::handle<HLRBRep_Data>& ds = out.algo->DataStructure();
  const HLRBRep_Curve& gc = ds->EDataArray()(hit->source_entry_index).Geometry();
  double xs[2];
  for (int k = 0; k < 2; ++k) {
    const gp_Pnt p3 = gc.Value3D(k == 0 ? iv.first : iv.second);
    xs[k] = p3.X();
  }
  const double lo = std::min(xs[0], xs[1]);
  const double hi = std::max(xs[0], xs[1]);
  if (!expect(std::abs(lo - 40.0) < 1e-3 && std::abs(hi - 100.0) < 1e-3,
              "interval: visible interval maps to x in [40, 100]")) {
    return false;
  }

  // The emitted piece's projected endpoints must land at x=40/100 in
  // view space (view X = world X, view Y = world Z).
  const double exs[2] = {hit->p_start.X(), hit->p_end.X()};
  const double elo = std::min(exs[0], exs[1]);
  const double ehi = std::max(exs[0], exs[1]);
  return expect(std::abs(elo - 40.0) < 1e-3 && std::abs(ehi - 100.0) < 1e-3,
                "interval: emitted piece covers exactly the visible run");
}

// ── Test 4: parameter mapping round-trip ────────────────────────────

bool test_parameter_mapping_round_trip() {
  // Bare slab: all front edges fully visible.  For a straight source
  // edge, Parameter3d(Parameter2d(t)) must round-trip and the 2D
  // value must equal the projected 3D point.
  TopoDS_Shape fused =
      BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(100, 10, 30)).Shape();

  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0), gp_Dir(1, 0, 0));

  occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
  algo->Add(fused, 0);
  HLRAlgo_Projector projector(frame);
  algo->Projector(projector);
  algo->Update();
  algo->Hide();

  const occ::handle<HLRBRep_Data>& ds = algo->DataStructure();
  const auto& edata = ds->EDataArray();
  bool mapped = false;
  for (int i = 1; i <= ds->NbEdges() && !mapped; ++i) {
    const HLRBRep_Curve& gc = edata(i).Geometry();
    if (gc.GetCurve().GetType() != GeomAbs_Line) continue;
    const double t0 = gc.FirstParameter();
    const double t1 = gc.LastParameter();
    const double tmid = (t0 + t1) / 2.0;
    const double p2d = gc.Parameter2d(tmid);
    const double p3d = gc.Parameter3d(p2d);
    if (!expect(std::abs(p3d - tmid) < 1e-6,
                "param map: Parameter3d(Parameter2d(t)) round-trips")) {
      return false;
    }
    // The 2D value is the VIEW projection of the 3D point (view Y is
    // world Z for this frame) — compare through the projector.
    const gp_Pnt2d v2 = gc.Value(tmid);
    const gp_Pnt v3 = gc.Value3D(tmid);
    gp_Pnt2d projected;
    projector.Project(v3, projected);
    if (!expect(std::abs(v2.X() - projected.X()) < 1e-6 &&
                    std::abs(v2.Y() - projected.Y()) < 1e-6,
                "param map: 2D value equals projected 3D point")) {
      return false;
    }
    mapped = true;
  }
  return expect(mapped, "param map: at least one line edge was exercised");
}

// ── Test 5: coincident-edge de-duplication ──────────────────────────

bool test_coincident_edges_dedupe() {
  // Two separately-built identical boxes added as separate HLR
  // shapes (assembly-style multi-body view): HLR does NOT eliminate
  // superimposed lines, so the record set must contain duplicate
  // geometry.  The P2 de-dupe merges them by geometry signature and
  // concatenates the source lists (each signature gains 2 distinct
  // source edges).
  TopoDS_Shape box1 =
      BRepPrimAPI_MakeBox(gp_Pnt(-5, -10, -15), gp_Pnt(5, 10, 15)).Shape();
  TopoDS_Shape box2 =
      BRepPrimAPI_MakeBox(gp_Pnt(-5, -10, -15), gp_Pnt(5, 10, 15)).Shape();

  occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
  algo->Add(box1, 0);
  algo->Add(box2, 0);  // coincident second body
  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
  HLRAlgo_Projector projector(frame);
  algo->Projector(projector);
  algo->Update();
  algo->Hide();

  occ::handle<HLRBRep_Data> ds = algo->DataStructure();
  auto& edata = ds->EDataArray();
  std::vector<EdgeRecord> records;
  for (int i = 1; i <= ds->NbEdges(); ++i) {
    const HLRBRep_Curve& gc = edata(i).Geometry();
    HLRAlgo_EdgeStatus& st = edata(i).Status();
    const TopoDS_Shape& mapped = ds->EdgeMap()(i);
    if (st.AllHidden()) continue;
    for (int vp = 1; vp <= st.NbVisiblePart(); ++vp) {
      double s, e;
      float ts, te;
      st.VisiblePart(vp, s, ts, e, te);
      const TopoDS_Edge piece = HLRBRep::MakeEdge(gc, s, e);
      if (piece.IsNull()) continue;
      EdgeRecord rec;
      rec.line_class = "visible";
      rec.curve_class = "sharp";
      rec.has_source_edge = !mapped.IsNull() && mapped.ShapeType() == TopAbs_EDGE;
      if (rec.has_source_edge) rec.source_edge = TopoDS::Edge(mapped);
      BRepAdaptor_Curve out(piece);
      rec.curve_kind = out.GetType() == GeomAbs_Line ? "line" : "other";
      rec.first_param = out.FirstParameter();
      rec.last_param = out.LastParameter();
      const gp_Pnt q0 = out.Value(rec.first_param);
      const gp_Pnt q1 = out.Value(rec.last_param);
      const gp_Pnt qm = out.Value((rec.first_param + rec.last_param) / 2.0);
      rec.p_start = gp_Pnt2d(q0.X(), q0.Y());
      rec.p_end = gp_Pnt2d(q1.X(), q1.Y());
      rec.p_mid = gp_Pnt2d(qm.X(), qm.Y());
      records.push_back(rec);
    }
  }
  if (!expect(records.size() >= 4, "dedupe: duplicated visible records exist")) {
    return false;
  }
  std::vector<std::string> keys;
  for (const auto& rec : records) keys.push_back(record_key(rec));
  std::sort(keys.begin(), keys.end());
  const size_t unique = std::unique(keys.begin(), keys.end()) - keys.begin();
  std::cout << "dedupe: " << records.size() << " visible records, "
            << unique << " unique signatures\n";
  if (!expect(unique < records.size(),
              "dedupe: duplicates collapse to fewer unique signatures")) {
    return false;
  }
  // Merging by signature must concatenate distinct sources: within a
  // signature group no two records share an IsSame source edge.
  bool all_distinct = true;
  for (size_t i = 0; i < keys.size();) {
    size_t j = i;
    while (j < keys.size() && keys[j] == keys[i]) ++j;
    for (size_t a = i; a < j && all_distinct; ++a) {
      for (size_t b = a + 1; b < j; ++b) {
        if (records[a].source_edge.IsSame(records[b].source_edge)) {
          all_distinct = false;
          break;
        }
      }
    }
    i = j;
  }
  return expect(all_distinct,
                "dedupe: merged signatures carry distinct source edges");
}

// ── Test 6: hidden-line provenance ──────────────────────────────────

bool test_hidden_line_provenance() {
  // Box behind box: A at x=-10..0 fully occluded by B at x=0..10.
  TopoDS_Shape a =
      BRepPrimAPI_MakeBox(gp_Pnt(-10, -10, -10), gp_Pnt(0, 10, 10)).Shape();
  TopoDS_Shape b =
      BRepPrimAPI_MakeBox(gp_Pnt(0, -10, -10), gp_Pnt(10, 10, 10)).Shape();
  TopoDS_Shape fused = BRepAlgoAPI_Fuse(a, b).Shape();

  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
  HlrOutput out = run_hlr(fused, frame, /*verbose=*/true);

  int hidden_with_source = 0;
  for (const auto& rec : out.records) {
    if (rec.line_class == "hidden") {
      if (!expect(rec.has_source_edge,
                  "hidden: hidden records carry source edges")) {
        return false;
      }
      ++hidden_with_source;
    }
  }
  if (!expect(hidden_with_source > 0,
              "hidden: occluded box produces hidden records")) {
    return false;
  }
  // The back box (A) contributes only hidden records; B's front-face
  // edges are visible.
  const gp_Pnt b_front[4] = {
      gp_Pnt(10, -10, -10), gp_Pnt(10, 10, -10),
      gp_Pnt(10, 10, 10), gp_Pnt(10, -10, 10),
  };
  std::vector<TopoDS_Edge> b_front_edges;
  for (int i = 0; i < 4; ++i) {
    const auto fe = find_line_edges(fused, b_front[i], b_front[(i + 1) % 4], 1e-6);
    b_front_edges.insert(b_front_edges.end(), fe.begin(), fe.end());
  }
  if (!expect(b_front_edges.size() == 4,
              "hidden: B's front face has 4 edges")) {
    return false;
  }
  int b_front_visible = 0;
  for (const auto& rec : out.records) {
    if (rec.line_class == "visible" && rec.has_source_edge &&
        edge_in_set(rec.source_edge, b_front_edges)) {
      ++b_front_visible;
    }
  }
  return expect(b_front_visible == 4,
                "hidden: B's front edges all visible");
}

// ── Test 7: determinism ─────────────────────────────────────────────

bool test_determinism() {
  TopoDS_Shape a =
      BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(100, 10, 30)).Shape();
  TopoDS_Shape b =
      BRepPrimAPI_MakeBox(gp_Pnt(0, 20, 0), gp_Pnt(40, 30, 80)).Shape();
  TopoDS_Shape fused = BRepAlgoAPI_Fuse(a, b).Shape();
  gp_Ax2 frame(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0), gp_Dir(1, 0, 0));
  const std::string first = canonical_records(run_hlr(fused, frame, false));
  const std::string second = canonical_records(run_hlr(fused, frame, false));
  return expect(first == second,
                "determinism: identical input gives identical sorted output");
}

}  // namespace

int main() {
  if (!test_box_front_projection()) return 1;
  if (!test_silhouette_provenance()) return 1;
  if (!test_visible_interval_split()) return 1;
  if (!test_parameter_mapping_round_trip()) return 1;
  if (!test_coincident_edges_dedupe()) return 1;
  if (!test_hidden_line_provenance()) return 1;
  if (!test_determinism()) return 1;

  std::cout << "hlr_provenance_test passed\n";
  return 0;
}
