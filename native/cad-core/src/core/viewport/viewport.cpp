#include "core/viewport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <GProp_GProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "core/body_compiler.h"
#include "core/appearance.h"
#include "core/dof_counter.h"
#include "core/feature_shape.h"
#include "core/refresh_dependents.h"
#include "core/sketch_profile.h"

namespace polysmith::core {
namespace {

#include "core/viewport/impl/common_helpers.inc"
#include "core/viewport/impl/sketch_primitives.inc"
#include "core/viewport/impl/body_shape_helpers.inc"

}  // namespace

ViewportState build_viewport_state(const std::optional<DocumentState>& document) {
  if (!document.has_value()) {
    return ViewportState{
        .has_active_document = false,
        .boxes = {},
        .cylinders = {},
        .polygon_extrudes = {},
        .solid_faces = {},
        .reference_planes = {},
        .reference_axes = {},
        .reference_points = {},
        .helices = {},
        .sketch_lines = {},
        .sketch_circles = {},
        .sketch_arcs = {},
        .sketch_points = {},
        .sketch_dimensions = {},
        .sketch_constraints = {},
        .sketch_profiles = {},
        .dof_statuses = {},
        .meshes = {},
        .cut_previews = {},
        .bodies = {},
        .edges = {},
        .vertices = {},
        .toolpaths = {},
        .scene_width = 0.0,
        .scene_height = 0.0,
        .scene_depth = 0.0,
        .scene_bounds =
            ViewportSceneBounds{
                .center_x = 0.0,
                .center_y = 0.0,
                .center_z = 0.0,
                .width = 0.0,
                .height = 0.0,
            .depth = 0.0,
            .max_dimension = 0.0,
            },
        .selection_filter = SelectionFilter{},
    };
  }

  const DocumentState viewport_document =
      document_at_timeline_cursor(document.value());
  const DocumentState* view = &viewport_document;

  std::vector<ViewportBoxPrimitive> boxes;
  std::vector<ViewportCylinderPrimitive> cylinders;
  std::vector<ViewportPolygonExtrudePrimitive> polygon_extrudes;
  std::vector<ViewportSolidFace> solid_faces;
  std::vector<ViewportReferencePlane> reference_planes;
  std::vector<ViewportReferenceAxis> reference_axes;
  std::vector<ViewportReferencePoint> reference_points;
  std::vector<ViewportHelixPrimitive> helices;
  std::vector<ViewportSketchLinePrimitive> sketch_lines;
  std::vector<ViewportSketchCirclePrimitive> sketch_circles;
  std::vector<ViewportSketchPolygonPrimitive> sketch_polygons;
  std::vector<ViewportSketchArcPrimitive> sketch_arcs;
  std::vector<ViewportSketchPointPrimitive> sketch_points;
  std::vector<ViewportSketchDimensionPrimitive> sketch_dimensions;
  std::vector<ViewportSketchConstraintPrimitive> sketch_constraints;
  std::vector<ViewportSketchProfilePrimitive> sketch_profiles;
  std::vector<ViewportMeshPrimitive> meshes;
  std::vector<ViewportCutPreview> cut_previews;
  std::vector<ViewportBodySummary> bodies;
  std::vector<ViewportEdgePrimitive> edges;
  std::vector<ViewportVertexPrimitive> vertices;
  std::vector<EntityDofResult> dof_statuses;
  double current_x_offset = 0.0;
  double scene_width = 0.0;
  double max_height = 0.0;
  double max_depth = 0.0;

  // Walk the feature history once with boolean operators applied so we
  // know which features get consumed by Fuse/Cut and which bodies need
  // to be tessellated as mesh primitives. Features in the resulting
  // `consumed_feature_ids` set must be skipped by the legacy primitive
  // emission below to avoid double-rendering. Failures in OCCT booleans
  // produce empty meshes — see body_compiler.cpp — so legacy fallback
  // still renders something.
  CompiledBodies compiled_bodies = compile_bodies(*view);
  std::optional<std::string> selected_move_target_body_id;
  if (view->selected_feature_id.has_value()) {
    for (const auto& feature : view->feature_history) {
      if (feature.id == view->selected_feature_id.value() &&
          feature.kind == "move" &&
          feature.move_parameters.has_value()) {
        selected_move_target_body_id = feature.move_parameters->target_body_id;
        break;
      }
    }
  }
  for (const auto& body_mesh : compiled_bodies.meshes) {
    ViewportMeshPrimitive mesh{};
    mesh.id = body_mesh.body_id;
    mesh.positions = body_mesh.vertices;
    mesh.normals = body_mesh.normals;
    mesh.indices = body_mesh.indices;
    mesh.is_selected =
        view->selected_feature_id.has_value() &&
        (view->selected_feature_id.value() == body_mesh.body_id ||
         (selected_move_target_body_id.has_value() &&
          selected_move_target_body_id.value() == body_mesh.body_id));
    mesh.appearance_color = body_appearance_color(*view, body_mesh.body_id);
    meshes.push_back(std::move(mesh));
  }
  const std::set<std::string>& consumed = compiled_bodies.consumed_feature_ids;

  // Cut preview overlay: when the user has a cut extrude selected (i.e.
  // the floating Extrude panel is open and editing it), emit a
  // translucent red mesh of the cutter volume so they can see exactly
  // what's about to be removed. This is a UI overlay only — the
  // booleaned body itself already renders the post-cut shape via
  // `meshes`. We only emit the preview while the feature is the
  // currently-selected one to avoid clutter on saved documents.
  if (view->selected_feature_id.has_value()) {
    for (const auto& feature : view->feature_history) {
      if (feature.id != view->selected_feature_id.value()) {
        continue;
      }
      if (feature.kind != "extrude" ||
          !feature.extrude_parameters.has_value() ||
          feature.extrude_parameters->mode != "cut") {
        break;
      }
      try {
        const TopoDS_Shape cutter =
            build_extrude_shape(feature.extrude_parameters.value());
        if (cutter.IsNull()) {
          break;
        }
        ViewportCutPreview preview{};
        preview.id = feature.id;
        tessellate_shape_to_arrays(cutter,
                                   preview.positions,
                                   preview.normals,
                                   preview.indices);
        if (!preview.positions.empty() && !preview.indices.empty()) {
          cut_previews.push_back(std::move(preview));
        }
      } catch (const std::exception&) {
        // Cutter build failures shouldn't break the rest of the
        // viewport; just skip the preview for this snapshot.
      }
      break;
    }
  }

  // Build the body summary list for the UI's target picker. The body's
  // root id maps 1:1 to a feature id, so we look up the human-readable
  // name from feature_history; missing or empty names degrade to the id
  // itself so the picker is always populated.
  for (const auto& body : compiled_bodies.bodies) {
    std::string label = body.id;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body.id && !feature.name.empty()) {
        label = feature.name;
        break;
      }
    }
    const BodyBounds bounds = bounds_for_shape(body.shape);
    bodies.push_back(ViewportBodySummary{
        .id = body.id,
        .label = label,
        .center_x = bounds.center_x,
        .center_y = bounds.center_y,
        .center_z = bounds.center_z,
        .width = bounds.width,
        .height = bounds.height,
        .depth = bounds.depth,
        .local_frame = body.local_frame,
    });
    // Edge picking uses pick_shape when the body has one (set by
    // body_compiler for the duration of a pending fillet/chamfer
    // panel session). This keeps edge ids stable while the user
    // toggles edges, even though body.shape is mutating with each
    // update_*_edges. Vertices and faces still come from the live
    // post-op shape because vertex / face picks aren't part of the
    // pending feature's input set.
    const TopoDS_Shape& edge_pick_shape =
        body.pick_shape.IsNull() ? body.shape : body.pick_shape;
    enumerate_body_edges(edge_pick_shape,
                         body.id,
                         view->selected_edge_ids,
                         edges);
    enumerate_body_vertices(body.shape,
                            body.id,
                            view->selected_vertex_ids,
                            vertices);
    // Look up the body's owning feature kind so the face's owner_kind
    // stays useful to consumers (the UI uses it to label faces).
    std::string body_kind;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body.id) {
        body_kind = feature.kind;
        break;
      }
    }
    // Legacy box/cylinder features render at a `current_x_offset` that
    // body_compiler doesn't know about (their shapes are built at the
    // origin). Body-derived faces would therefore land away from the
    // visual primitive — keep their analytical faces (emitted below in
    // the per-feature loop) and skip body-derived faces for them.
    if (body_kind != "box" && body_kind != "cylinder") {
      enumerate_body_faces(body.shape,
                           body.id,
                           body_kind,
                           *view,
                           view->selected_face_id,
                           solid_faces);
    }
  }


#include "core/viewport/impl/feature_history_emit.inc"

  // Drop legacy named-suffix analytical faces for extrude features
  // (e.g. "<id>:face:top", "<id>:face:base", "<id>:face:side-N"). The
  // per-feature loop above still emits those for backwards
  // compatibility with the old tests / serialization paths, but every
  // extrude body now also gets accurate body-derived faces from
  // `enumerate_body_faces` (with numeric suffixes "<id>:face:0",
  // "<id>:face:1", ...). When both are present they overlap as
  // transparent meshes at nearly-identical world positions, producing
  // a "ghost plane" the user can't easily click through. Body-derived
  // faces always win because they handle filleting, plane-frame
  // rotations, and booleaned topology correctly — analytical ones
  // only ever matched on the simple new-body case.
  {
    auto is_named_suffix_for_extrude =
        [&](const ViewportSolidFace& face) -> bool {
      if (face.owner_kind != "extrude") {
        return false;
      }
      // Find the suffix after the last ":face:" delimiter and check
      // whether it parses as a non-negative integer. Numeric -> body-
      // derived (keep). Non-numeric -> legacy analytical (drop).
      const std::string separator = ":face:";
      const auto pos = face.face_id.rfind(separator);
      if (pos == std::string::npos) {
        return false;
      }
      const std::string suffix =
          face.face_id.substr(pos + separator.size());
      if (suffix.empty()) {
        return false;
      }
      for (const char ch : suffix) {
        if (ch < '0' || ch > '9') {
          return true;
        }
      }
      return false;
    };
    solid_faces.erase(std::remove_if(solid_faces.begin(),
                                     solid_faces.end(),
                                     is_named_suffix_for_extrude),
                      solid_faces.end());
  }

  const ViewportSceneBounds scene_bounds = {
      .center_x = scene_width_with_references / 2.0,
      .center_y = scene_height_with_references / 2.0,
      .center_z = scene_depth_with_references / 2.0,
      .width = scene_width_with_references,
      .height = scene_height_with_references,
      .depth = scene_depth_with_references,
      .max_dimension = std::max({scene_width_with_references,
                                 scene_height_with_references,
                                 scene_depth_with_references}),
  };

  // Build pre-computed snap candidates for the active sketch.
  std::vector<SnapCandidate> snap_candidates;
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        const auto& sketch = feat.sketch_parameters.value();
        const auto& filter = document->selection_filter;
        // Endpoints
        if (filter.snap_endpoint) {
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="endpoint",.entity_id=line.id,.point_id=line.start_point_id,.local_x=line.start_x,.local_y=line.start_y,.distance=0,.label="Endpoint"});
            snap_candidates.push_back({.kind="endpoint",.entity_id=line.id,.point_id=line.end_point_id,.local_x=line.end_x,.local_y=line.end_y,.distance=0,.label="Endpoint"});
          }
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="endpoint",.entity_id=arc.id,.point_id=arc.start_point_id,.local_x=arc.start_x,.local_y=arc.start_y,.distance=0,.label="Endpoint"});
            snap_candidates.push_back({.kind="endpoint",.entity_id=arc.id,.point_id=arc.end_point_id,.local_x=arc.end_x,.local_y=arc.end_y,.distance=0,.label="Endpoint"});
          }
        }
        // Midpoints
        if (filter.snap_midpoint) {
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            const double mx = (line.start_x + line.end_x) / 2.0;
            const double my = (line.start_y + line.end_y) / 2.0;
            snap_candidates.push_back({.kind="midpoint",.entity_id=line.id,.point_id="",.local_x=mx,.local_y=my,.distance=0,.label="Midpoint"});
          }
        }
        // Centers
        if (filter.snap_center) {
          for (const auto& circle : sketch.circles) {
            if (circle.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=circle.id,.point_id="",.local_x=circle.center_x,.local_y=circle.center_y,.distance=0,.label="Center"});
          }
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=arc.id,.point_id="",.local_x=arc.center_x,.local_y=arc.center_y,.distance=0,.label="Center"});
          }
          for (const auto& poly : sketch.polygons) {
            if (poly.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=poly.id,.point_id="",.local_x=poly.center_x,.local_y=poly.center_y,.distance=0,.label="Center"});
          }
        }
        // Quadrant points (4 per circle)
        if (filter.snap_quadrant) {
          for (const auto& circle : sketch.circles) {
            if (circle.is_construction && !filter.select_construction) continue;
            const double cx = circle.center_x;
            const double cy = circle.center_y;
            const double r = circle.radius;
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx+r,.local_y=cy,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx,.local_y=cy+r,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx-r,.local_y=cy,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.point_id="",.local_x=cx,.local_y=cy-r,.distance=0,.label="Quadrant"});
          }
        }
        // Intersection points (line-line and line-arc)
        if (filter.snap_intersection) {
          // Line-line intersections
          for (size_t i = 0; i < sketch.lines.size(); ++i) {
            const auto& a = sketch.lines[i];
            if (a.is_construction && !filter.select_construction) continue;
            for (size_t j = i + 1; j < sketch.lines.size(); ++j) {
              const auto& b = sketch.lines[j];
              if (b.is_construction && !filter.select_construction) continue;
              const double a_dx = a.end_x - a.start_x;
              const double a_dy = a.end_y - a.start_y;
              const double b_dx = b.end_x - b.start_x;
              const double b_dy = b.end_y - b.start_y;
              const double denom = a_dx * b_dy - a_dy * b_dx;
              if (std::abs(denom) < 1e-12) continue;
              const double t = ((b.start_x - a.start_x) * b_dy - (b.start_y - a.start_y) * b_dx) / denom;
              const double u = ((b.start_x - a.start_x) * a_dy - (b.start_y - a.start_y) * a_dx) / denom;
              if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) continue;
              snap_candidates.push_back({.kind="intersection",.entity_id=a.id,.point_id="",.local_x=a.start_x+t*a_dx,.local_y=a.start_y+t*a_dy,.distance=0,.label="Intersection"});
            }
          }
          // Line-arc intersections
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            const double dx = line.end_x - line.start_x;
            const double dy = line.end_y - line.start_y;
            const double len_sq = dx * dx + dy * dy;
            if (len_sq < 1e-12) continue;
            for (const auto& arc : sketch.arcs) {
              if (arc.is_construction && !filter.select_construction) continue;
              const double r = std::hypot(arc.start_x - arc.center_x, arc.start_y - arc.center_y);
              const double fx = line.start_x - arc.center_x;
              const double fy = line.start_y - arc.center_y;
              const double a_val = len_sq;
              const double b_val = 2.0 * (fx * dx + fy * dy);
              const double c_val = fx * fx + fy * fy - r * r;
              double disc = b_val * b_val - 4.0 * a_val * c_val;
              if (disc < 0) continue;
              disc = std::sqrt(disc);
              for (double sign : {-1.0, 1.0}) {
                const double t = (-b_val + sign * disc) / (2.0 * a_val);
                if (t < 0.0 || t > 1.0) continue;
                snap_candidates.push_back({.kind="intersection",.entity_id=line.id,.point_id="",.local_x=line.start_x+t*dx,.local_y=line.start_y+t*dy,.distance=0,.label="Intersection"});
              }
            }
          }
        }
        // grid_line, polar, tangent, nearest, and perpendicular are
        // cursor-position-dependent and handled by the TS dynamic snap
        // path — they are not pre-computed here.
        break;
      }
    }
  }

  // Populate DOF statuses for the active sketch.
  int solver_dofs = -1;
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        dof_statuses = count_sketch_dof(feat.sketch_parameters.value());
        solver_dofs = feat.sketch_parameters->solver_dofs;
        break;
      }
    }
  }

  for (auto& face : solid_faces) {
    if (!face.appearance_color.has_value()) {
      face.appearance_color =
          face_appearance_color(*view, face.face_id, "semantic:" + face.face_id);
    }
  }

  // ── CAM toolpaths from operations ────────────────────────────
  std::vector<ViewportToolpathPrimitive> toolpaths;
  for (const auto& op : view->cam_operations) {
    CamToolpath camTp;
    if (op.type == CamOperationType::FaceMilling) {
      camTp = generate_face_milling_toolpath(op, *view);
    }
    // Convert CamToolpath to ViewportToolpathPrimitive.
    if (camTp.totalPoints > 0) {
      ViewportToolpathPrimitive vtp;
      vtp.id = op.id;
      vtp.label = op.name;
      for (const auto& move : camTp.moves) {
        for (const auto& pt : move.points) {
          vtp.points.push_back({pt.x, pt.y, pt.z, move.isRapid});
        }
      }
      toolpaths.push_back(std::move(vtp));
    }
  }

  // Toolpaths are only emitted when CAM operations exist.

  // ── CAM stock — deferred until non-interactive rendering is available.
  // TODO: render stock as a translucent box that doesn't participate in
  // the CAD picking chain (ViewportBoxPrimitive triggers feature lookup).

  return ViewportState{
      .has_active_document = true,
      .boxes = boxes,
      .cylinders = cylinders,
      .polygon_extrudes = polygon_extrudes,
      .solid_faces = solid_faces,
      .reference_planes = reference_planes,
      .reference_axes = reference_axes,
      .reference_points = reference_points,
      .helices = helices,
      .sketch_lines = sketch_lines,
      .sketch_circles = sketch_circles,
      .sketch_polygons = sketch_polygons,
      .sketch_arcs = sketch_arcs,
      .sketch_points = sketch_points,
      .sketch_dimensions = sketch_dimensions,
      .sketch_constraints = sketch_constraints,
      .sketch_profiles = sketch_profiles,
      .dof_statuses = dof_statuses,
      .solver_dofs = solver_dofs,
      .meshes = meshes,
      .cut_previews = cut_previews,
      .bodies = bodies,
      .edges = edges,
      .vertices = vertices,
      .toolpaths = toolpaths,
      .scene_width = scene_width_with_references,
      .scene_height = scene_height_with_references,
      .scene_depth = scene_depth_with_references,
      .scene_bounds = scene_bounds,
      .snap_candidates = snap_candidates,
      .selection_filter = document.has_value()
          ? document->selection_filter
          : SelectionFilter{},
  };
}

}  // namespace polysmith::core
