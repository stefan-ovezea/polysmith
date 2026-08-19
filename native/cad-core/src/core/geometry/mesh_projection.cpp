#include "core/geometry/mesh_projection.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "core/sketch/sketch_feature_parameters.h"

namespace polysmith::core {
namespace {

using Segment = std::pair<gp_Pnt, gp_Pnt>;

// Snap tolerance for chaining endpoints into loops. STL triangles share
// vertices exactly, so 1e-6 is generous while still separating
// genuinely distinct points.
constexpr double kSnapTolerance = 1e-6;

// Polyline simplification tolerance (mm): below STL facet resolution
// and 3D-print tolerance.
constexpr double kPolylineSimplifyTolerance = 0.05;

// A snapped point key: coordinates rounded to the snap grid.
struct PointKey {
  std::int64_t x;
  std::int64_t y;
  std::int64_t z;

  bool operator==(const PointKey& other) const {
    return x == other.x && y == other.y && z == other.z;
  }

  bool operator<(const PointKey& other) const {
    if (x != other.x) return x < other.x;
    if (y != other.y) return y < other.y;
    return z < other.z;
  }
};

struct PointKeyHash {
  std::size_t operator()(const PointKey& key) const {
    std::size_t hash = std::hash<std::int64_t>()(key.x);
    hash = hash * 31 + std::hash<std::int64_t>()(key.y);
    hash = hash * 31 + std::hash<std::int64_t>()(key.z);
    return hash;
  }
};

PointKey snap_key(const gp_Pnt& point) {
  return PointKey{
      .x = static_cast<std::int64_t>(std::llround(point.X() / kSnapTolerance)),
      .y = static_cast<std::int64_t>(std::llround(point.Y() / kSnapTolerance)),
      .z = static_cast<std::int64_t>(std::llround(point.Z() / kSnapTolerance)),
  };
}

// A shared mesh edge keyed by its snapped 3D endpoints. Adjacent
// triangles of different faces share coordinates (MakeShapeOnMesh
// builds shared edges), so the 3D key merges them across face
// boundaries.
struct EdgeKey {
  PointKey first;
  PointKey second;

  bool operator==(const EdgeKey& other) const {
    return first == other.first && second == other.second;
  }
};

struct EdgeKeyHash {
  std::size_t operator()(const EdgeKey& key) const {
    return PointKeyHash()(key.first) * 31 + PointKeyHash()(key.second);
  }
};

EdgeKey make_edge_key(const gp_Pnt& a, const gp_Pnt& b) {
  const PointKey ka = snap_key(a);
  const PointKey kb = snap_key(b);
  if (ka.x < kb.x || (ka.x == kb.x && (ka.y < kb.y || (ka.y == kb.y && ka.z <= kb.z)))) {
    return EdgeKey{ka, kb};
  }
  return EdgeKey{kb, ka};
}

// Collect the edges of `shape` as world-space segments. Straight edges
// contribute their endpoints; anything else (defensive — sections of
// planar facets are always linear) is discretized.
void append_shape_segments(const TopoDS_Shape& shape,
                           std::vector<Segment>& segments,
                           const gp_Trsf* transform = nullptr) {
  for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) {
    const TopoDS_Edge& edge = TopoDS::Edge(exp.Current());
    const BRepAdaptor_Curve curve(edge);
    if (curve.GetType() == GeomAbs_Line) {
      gp_Pnt first = curve.Value(curve.FirstParameter());
      gp_Pnt last = curve.Value(curve.LastParameter());
      if (transform != nullptr) {
        first.Transform(*transform);
        last.Transform(*transform);
      }
      segments.emplace_back(first, last);
    } else {
      constexpr int kDiscretization = 8;
      gp_Pnt previous = curve.Value(curve.FirstParameter());
      for (int i = 1; i <= kDiscretization; ++i) {
        const double t =
            curve.FirstParameter() +
            (curve.LastParameter() - curve.FirstParameter()) * i /
                kDiscretization;
        gp_Pnt current = curve.Value(t);
        if (transform != nullptr) {
          previous.Transform(*transform);
          current.Transform(*transform);
        }
        segments.emplace_back(previous, current);
        previous = curve.Value(t);
      }
    }
  }
}

// True when (a, b, c) are collinear and b lies between a and c. Used
// to merge collinear continuation segments (triangle diagonals split
// section edges into collinear halves) into single sketch lines.
bool collinear_mergeable(const gp_Pnt& a, const gp_Pnt& b,
                         const gp_Pnt& c) {
  const gp_Vec first(b, a);
  const gp_Vec second(c, b);
  const gp_Vec cross = first.Crossed(second);
  // Exact collinearity up to 1e-8 (mm-scale float32 STL input).
  if (cross.SquareMagnitude() > 1e-16) {
    return false;
  }
  return first.Dot(second) > 0.0;  // b strictly between a and c
}

// Chain unordered segments into polylines by endpoint matching, merging
// collinear continuations along the way. Closed chains become loops
// (last point == first); open chains are emitted as-is (v1 allows open
// silhouette fragments).
std::vector<std::vector<gp_Pnt>> chain_segments(
    const std::vector<Segment>& segments) {
  std::vector<Segment> kept;
  kept.reserve(segments.size());
  for (const auto& segment : segments) {
    if (segment.first.SquareDistance(segment.second) >
        kSnapTolerance * kSnapTolerance) {
      kept.push_back(segment);
    }
  }
  if (kept.empty()) {
    return {};
  }

  // Deduplicate identical segments: the same projected boundary edge
  // arrives from several faces (a box's top-cap, side, and bottom-cap
  // edges all project onto the footprint sides). Duplicates would
  // otherwise make the greedy walk below close degenerate 2-point
  // loops instead of continuing around the outline.
  {
    std::unordered_set<EdgeKey, EdgeKeyHash> seen_keys;
    std::vector<Segment> deduped;
    deduped.reserve(kept.size());
    for (const auto& segment : kept) {
      if (seen_keys
              .insert(make_edge_key(segment.first, segment.second))
              .second) {
        deduped.push_back(segment);
      }
    }
    kept = std::move(deduped);
  }
  if (kept.empty()) {
    return {};
  }

  // Index endpoints -> segment indices.
  std::unordered_map<PointKey, std::vector<int>, PointKeyHash> endpoints;
  for (int i = 0; i < static_cast<int>(kept.size()); ++i) {
    endpoints[snap_key(kept[i].first)].push_back(i);
    endpoints[snap_key(kept[i].second)].push_back(i);
  }

  std::vector<bool> used(kept.size(), false);
  std::vector<std::vector<gp_Pnt>> polylines;
  for (int start_index = 0; start_index < static_cast<int>(kept.size());
       ++start_index) {
    if (used[start_index]) {
      continue;
    }
    used[start_index] = true;

    std::vector<gp_Pnt> chain{kept[start_index].first,
                              kept[start_index].second};
    gp_Pnt front = chain.front();
    gp_Pnt back = chain.back();

    // Extend in both directions; a closed loop terminates when the
    // chain's ends meet.
    bool closed = snap_key(front) == snap_key(back);
    bool extended = true;
    while (extended && !closed) {
      extended = false;
      // Forward from the back.
      const auto& back_candidates = endpoints[snap_key(back)];
      for (const int index : back_candidates) {
        if (used[index]) {
          continue;
        }
        const Segment& segment = kept[index];
        gp_Pnt next;
        if (snap_key(segment.first) == snap_key(back)) {
          next = segment.second;
        } else if (snap_key(segment.second) == snap_key(back)) {
          next = segment.first;
        } else {
          continue;
        }
        used[index] = true;
        if (chain.size() >= 2 &&
            collinear_mergeable(chain[chain.size() - 2], back, next)) {
          chain.back() = next;  // merge collinear continuation
        } else {
          chain.push_back(next);
        }
        back = next;
        extended = true;
        break;
      }
      if (snap_key(front) == snap_key(back)) {
        closed = true;
        break;
      }
      if (extended) {
        continue;
      }
      // Backward from the front.
      const auto& front_candidates = endpoints[snap_key(front)];
      for (const int index : front_candidates) {
        if (used[index]) {
          continue;
        }
        const Segment& segment = kept[index];
        gp_Pnt next;
        if (snap_key(segment.first) == snap_key(front)) {
          next = segment.second;
        } else if (snap_key(segment.second) == snap_key(front)) {
          next = segment.first;
        } else {
          continue;
        }
        used[index] = true;
        if (chain.size() >= 2 &&
            collinear_mergeable(next, front, chain[1])) {
          chain.front() = next;  // merge collinear continuation
        } else {
          chain.insert(chain.begin(), next);
        }
        front = next;
        extended = true;
        break;
      }
      closed = snap_key(front) == snap_key(back);
    }

    // Closed loops keep the duplicated closing point (first == last)
    // so callers can tell loops from open chains: a polyline of N
    // points always emits N-1 line segments, and closedness is just
    // first == last.
    if (chain.size() >= 3) {
      polylines.push_back(std::move(chain));
    }
  }
  return polylines;
}

// Drop polylines that duplicate an earlier one's point set. A mesh
// seen along the projection direction often yields the same outline
// twice (e.g. the top and bottom footprint of a box project onto the
// identical rectangle) — emitting both would insert duplicate
// coincident sketch lines.
std::vector<std::vector<gp_Pnt>> dedupe_polylines(
    std::vector<std::vector<gp_Pnt>> polylines) {
  std::set<std::vector<PointKey>> seen;
  std::vector<std::vector<gp_Pnt>> result;
  result.reserve(polylines.size());
  for (auto& polyline : polylines) {
    std::vector<PointKey> keys;
    keys.reserve(polyline.size());
    for (const gp_Pnt& point : polyline) {
      keys.push_back(snap_key(point));
    }
    std::sort(keys.begin(), keys.end());
    if (seen.insert(std::move(keys)).second) {
      result.push_back(std::move(polyline));
    }
  }
  return result;
}

// Douglas-Peucker over one polyline. Closed polylines carry the
// duplicated closing point (first == last) and simplify over the loop
// with that point re-appended.
std::vector<gp_Pnt> simplify_polyline(const std::vector<gp_Pnt>& polyline,
                                      double tolerance) {
  const bool closed =
      polyline.size() >= 4 &&
      snap_key(polyline.front()) == snap_key(polyline.back());
  const size_t n = closed ? polyline.size() - 1 : polyline.size();
  if (n < 3 || tolerance <= 0.0) {
    return polyline;
  }

  const auto distance_to_segment = [](const gp_Pnt& p, const gp_Pnt& a,
                                      const gp_Pnt& b) {
    const gp_Vec ab(b, a);
    const double ab_sq = ab.SquareMagnitude();
    if (ab_sq <= 1e-24) {
      return gp_Vec(a, p).Magnitude();
    }
    const gp_Vec ap(a, p);
    const double t = std::clamp(ap.Dot(ab) / ab_sq, 0.0, 1.0);
    return gp_Vec(a.Translated(ab * t), p).Magnitude();
  };

  std::vector<bool> keep(n, false);
  keep[0] = true;
  struct Range {
    size_t lo;
    size_t hi;
  };
  // Closed: hi = n indexes the duplicated closing point. Open: the
  // last point is the far anchor.
  std::vector<Range> stack{closed ? Range{0, n} : Range{0, n - 1}};
  while (!stack.empty()) {
    const Range range = stack.back();
    stack.pop_back();
    double max_distance = -1.0;
    size_t farthest = range.lo + 1;
    for (size_t i = range.lo + 1; i < range.hi; ++i) {
      const double distance = distance_to_segment(
          polyline[i], polyline[range.lo], polyline[range.hi % polyline.size()]);
      if (distance > max_distance) {
        max_distance = distance;
        farthest = i;
      }
    }
    if (max_distance > tolerance) {
      keep[farthest % n] = true;
      stack.push_back({range.lo, farthest});
      stack.push_back({farthest, range.hi});
    }
  }

  std::vector<gp_Pnt> result;
  result.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    if (keep[i]) {
      result.push_back(polyline[i]);
    }
  }
  if (result.size() < 3) {
    return polyline;
  }
  if (closed) {
    result.push_back(result.front());
  }
  return result;
}

// Decimate every polyline (0.05 mm — below STL facet resolution and
// print tolerance) so dense chord samples of curved outlines don't
// flood the sketch with lines.
std::vector<std::vector<gp_Pnt>> simplify_polylines(
    std::vector<std::vector<gp_Pnt>> polylines, double tolerance) {
  for (auto& polyline : polylines) {
    polyline = simplify_polyline(polyline, tolerance);
  }
  return polylines;
}

// True when every vertex of `face` lies on `plane` within tolerance.
bool face_is_coplanar(const TopoDS_Face& face, const gp_Pln& plane,
                      double tolerance) {
  bool any_vertex = false;
  for (TopExp_Explorer exp(face, TopAbs_VERTEX); exp.More(); exp.Next()) {
    any_vertex = true;
    const gp_Pnt point =
        BRep_Tool::Pnt(TopoDS::Vertex(exp.Current()));
    if (std::abs(plane.Distance(point)) > tolerance) {
      return false;
    }
  }
  return any_vertex;
}

// Edges of the flat region the body forms where its faces lie IN the
// section plane (bodies sitting flat on the plane). Two products:
//   - `boundary`: the region outline — edges belonging to exactly ONE
//     coplanar face. These form the section outline of the flat part.
//   - `internal`: edges shared by TWO coplanar faces (triangle
//     diagonals, seams between fused plates). They must NOT reach the
//     sketch — the BOP section emits them too when the faces coincide
//     with the plane, so callers filter them out of the BOP result.
// Counting by snapped 3D key is robust whether the faces share
// topological edges or merely geometrically identical ones. Curved
// coplanar edges cannot occur here (STL facets are straight), but they
// are kept as boundary defensively.
struct CoplanarRegionEdges {
  std::vector<Segment> boundary;
  std::unordered_set<EdgeKey, EdgeKeyHash> internal;
};

CoplanarRegionEdges coplanar_region_edges(const TopoDS_Shape& mesh,
                                          const gp_Pln& plane) {
  std::unordered_map<EdgeKey, int, EdgeKeyHash> coplanar_edge_counts;
  std::vector<Segment> candidate_segments;
  for (TopExp_Explorer face_exp(mesh, TopAbs_FACE); face_exp.More();
       face_exp.Next()) {
    const TopoDS_Face& face = TopoDS::Face(face_exp.Current());
    if (!face_is_coplanar(face, plane, kSnapTolerance)) {
      continue;
    }
    for (TopExp_Explorer edge_exp(face, TopAbs_EDGE); edge_exp.More();
         edge_exp.Next()) {
      const TopoDS_Edge& edge = TopoDS::Edge(edge_exp.Current());
      const BRepAdaptor_Curve curve(edge);
      if (curve.GetType() != GeomAbs_Line) {
        append_shape_segments(edge, candidate_segments);
        continue;
      }
      const gp_Pnt first = curve.Value(curve.FirstParameter());
      const gp_Pnt last = curve.Value(curve.LastParameter());
      ++coplanar_edge_counts[make_edge_key(first, last)];
      candidate_segments.emplace_back(first, last);
    }
  }
  CoplanarRegionEdges result;
  result.boundary.reserve(candidate_segments.size());
  for (const auto& segment : candidate_segments) {
    const auto count =
        coplanar_edge_counts[make_edge_key(segment.first, segment.second)];
    if (count >= 2) {
      result.internal.insert(make_edge_key(segment.first, segment.second));
    } else {
      result.boundary.push_back(segment);
    }
  }
  return result;
}

}  // namespace

std::vector<std::vector<gp_Pnt>> compute_mesh_section_polylines(
    const TopoDS_Shape& mesh, const PlaneFrame& frame) {
  if (mesh.IsNull()) {
    return {};
  }

  const gp_Pln cut_plane(
      gp_Pnt(frame.origin_x, frame.origin_y, frame.origin_z),
      gp_Dir(frame.normal_x, frame.normal_y, frame.normal_z));

  // Coplanar faces (a body sitting flat on the plane) confuse the BOP
  // section: it emits every coincident face's wire, triangulation
  // diagonals included, so its result must be filtered. The region
  // boundary walk supplies the outline of the flat part — and is needed
  // unconditionally for partly-coplanar bodies (a panel face in the
  // plane with blades crossing it): the BOP section contributes the
  // crossing geometry, the boundary walk the panel outline.
  const CoplanarRegionEdges region = coplanar_region_edges(mesh, cut_plane);

  std::vector<Segment> segments;
  try {
    BRepAlgoAPI_Section section_op(mesh, cut_plane);
    section_op.ComputePCurveOn1(false);
    section_op.ComputePCurveOn2(false);
    section_op.Build();
    if (section_op.IsDone()) {
      std::vector<Segment> bop_segments;
      append_shape_segments(section_op.Shape(), bop_segments);
      for (const auto& segment : bop_segments) {
        if (!region.internal.contains(
                make_edge_key(segment.first, segment.second))) {
          segments.push_back(segment);
        }
      }
    }
  } catch (const Standard_Failure&) {
    return {};
  }

  segments.insert(segments.end(), region.boundary.begin(),
                  region.boundary.end());

  return simplify_polylines(
      dedupe_polylines(chain_segments(std::move(segments))),
      kPolylineSimplifyTolerance);
}

std::vector<std::vector<gp_Pnt>> compute_mesh_silhouette_polylines(
    const TopoDS_Shape& mesh, const PlaneFrame& frame) {
  if (mesh.IsNull()) {
    return {};
  }

  // Mesh-native silhouette (Fusion "Project" semantics): an edge is
  // part of the outline seen along the frame normal when its adjacent
  // triangles are not both on the same side of the viewing direction.
  // Edge-on faces (perpendicular to the view) participate via the
  // distinct-normal rule so box footprints work; boundary edges of
  // open meshes are always part of the outline.
  //
  // (OCCT's HLRBRep_Algo exact pipeline was tried first and crashed
  // with heap/stack corruption on mesh input — the manual criterion
  // is simpler and exactly right for triangles.)
  const gp_Vec view(frame.normal_x, frame.normal_y, frame.normal_z);
  const gp_Pnt plane_origin(frame.origin_x, frame.origin_y, frame.origin_z);
  const gp_Pln plane(plane_origin, view);
  constexpr double kSideEpsilon = 1e-12;

  // Signed distance from the plane along the frame normal (gp_Pln
  // only offers the absolute distance, which would push points on the
  // near side AWAY from the plane).
  const auto signed_plane_distance = [&](const gp_Pnt& point) {
    return gp_Vec(plane_origin, point).Dot(view);
  };

  struct EdgeRecord {
    gp_Pnt a;
    gp_Pnt b;
    std::vector<gp_Vec> normals;  // one per adjacent triangle
  };
  std::unordered_map<EdgeKey, EdgeRecord, EdgeKeyHash> edges;

  try {
    for (TopExp_Explorer face_exp(mesh, TopAbs_FACE); face_exp.More();
         face_exp.Next()) {
      const TopoDS_Face& face = TopoDS::Face(face_exp.Current());
      TopLoc_Location location;
      const occ::handle<Poly_Triangulation> triangulation =
          BRep_Tool::Triangulation(face, location);
      if (triangulation.IsNull()) {
        continue;
      }
      const gp_Trsf transform = location.Transformation();

      for (int t = 1; t <= triangulation->NbTriangles(); ++t) {
        int n1, n2, n3;
        triangulation->Triangle(t).Get(n1, n2, n3);
        gp_Pnt p0 = triangulation->Node(n1).Transformed(transform);
        gp_Pnt p1 = triangulation->Node(n2).Transformed(transform);
        gp_Pnt p2 = triangulation->Node(n3).Transformed(transform);

        const gp_Vec normal = gp_Vec(p0, p1).Crossed(gp_Vec(p0, p2));
        if (normal.SquareMagnitude() <= 1e-30) {
          continue;  // degenerate triangle
        }
        const gp_Vec unit_normal = normal / std::sqrt(normal.SquareMagnitude());

        const std::array<std::pair<gp_Pnt, gp_Pnt>, 3> sides{{
            {p0, p1}, {p1, p2}, {p2, p0},
        }};
        for (const auto& [a, b] : sides) {
          EdgeRecord& record = edges[make_edge_key(a, b)];
          if (record.normals.empty()) {
            record.a = a;
            record.b = b;
          }
          record.normals.push_back(unit_normal);
        }
      }
    }
  } catch (const Standard_Failure&) {
    return {};
  }

  std::vector<Segment> segments;
  for (const auto& [key, record] : edges) {
    (void)key;
    bool silhouette = false;
    if (record.normals.size() < 2) {
      silhouette = true;  // boundary edge of an open mesh
    } else {
      const double d0 = record.normals[0].Dot(view);
      const double d1 = record.normals[1].Dot(view);
      const bool both_front = d0 > kSideEpsilon && d1 > kSideEpsilon;
      const bool both_back = d0 < -kSideEpsilon && d1 < -kSideEpsilon;
      if (!both_front && !both_back) {
        if (std::abs(d0) <= kSideEpsilon && std::abs(d1) <= kSideEpsilon) {
          // Both faces edge-on: part of the outline only where the
          // surface actually turns (distinct normals), not along
          // coplanar diagonals inside a flat side face. The threshold
          // separates float32 normal wobble (~1e-14) from real corners
          // (~3e-4 for a 1-degree facet turn).
          silhouette = record.normals[0]
                           .Crossed(record.normals[1])
                           .SquareMagnitude() > 1e-8;
        } else {
          silhouette = true;
        }
      }
    }
    if (!silhouette) {
      continue;
    }
    // Project the edge onto the sketch plane (world space, in-plane).
    gp_Pnt a = record.a;
    gp_Pnt b = record.b;
    a.Translate(view.Multiplied(-signed_plane_distance(a)));
    b.Translate(view.Multiplied(-signed_plane_distance(b)));
    segments.emplace_back(a, b);
  }
  return simplify_polylines(dedupe_polylines(chain_segments(segments)),
                            kPolylineSimplifyTolerance);
}

std::optional<PlaneFrame> resolve_sketch_projection_frame(
    const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return PlaneFrame{
        .origin_x = frame.origin_x,
        .origin_y = frame.origin_y,
        .origin_z = frame.origin_z,
        .x_axis_x = frame.x_axis_x,
        .x_axis_y = frame.x_axis_y,
        .x_axis_z = frame.x_axis_z,
        .y_axis_x = frame.y_axis_x,
        .y_axis_y = frame.y_axis_y,
        .y_axis_z = frame.y_axis_z,
        .normal_x = frame.normal_x,
        .normal_y = frame.normal_y,
        .normal_z = frame.normal_z,
    };
  }

  // Origin ref-plane sketches store no frame — use the hardcoded
  // mapping (CAD-standard Z-up convention, mirrors
  // dependency_sketch_frame_helpers.inc):
  //   ref-plane-xy: local (x, y) -> world (x, y, 0), normal (0, 0, 1)
  //   ref-plane-yz: local (x, y) -> world (0, x, y), normal (1, 0, 0)
  //   ref-plane-xz: local (x, y) -> world (x, 0, y), normal (0, 1, 0)
  if (sketch.plane_id == "ref-plane-xy") {
    return PlaneFrame{0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
                      0.0, 0.0, 0.0, 1.0};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return PlaneFrame{0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0,
                      1.0, 1.0, 0.0, 0.0};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return PlaneFrame{0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0,
                      1.0, 0.0, 1.0, 0.0};
  }
  return std::nullopt;
}

std::pair<double, double> world_to_sketch_local(const PlaneFrame& frame,
                                                const gp_Pnt& point) {
  const double dx = point.X() - frame.origin_x;
  const double dy = point.Y() - frame.origin_y;
  const double dz = point.Z() - frame.origin_z;
  const double local_x = dx * frame.x_axis_x + dy * frame.x_axis_y +
                         dz * frame.x_axis_z;
  const double local_y = dx * frame.y_axis_x + dy * frame.y_axis_y +
                         dz * frame.y_axis_z;
  return {local_x, local_y};
}

}  // namespace polysmith::core
