import { z } from "zod";

import { planeFrameSchema } from "./common";

export const viewportStateSchema = z.object({
  has_active_document: z.boolean(),
  boxes: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      width: z.number(),
      height: z.number(),
      depth: z.number(),
      x_offset: z.number(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  cylinders: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      radius: z.number(),
      height: z.number(),
      x_offset: z.number(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  polygon_extrudes: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      plane_id: z.string(),
      plane_frame: z
        .object({
          origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable(),
      profile_points: z.array(
        z.object({
          x: z.number(),
          y: z.number(),
        }),
      ),
      inner_loops: z
        .array(z.array(z.object({ x: z.number(), y: z.number() })))
        .default([]),
      depth: z.number(),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  solid_faces: z.array(
    z.object({
      face_id: z.string(),
      owner_id: z.string(),
      owner_kind: z.string(),
      label: z.string(),
      sketchability: z.string(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      normal: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      plane_frame: z.object({
        origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
      }),
      size: z.object({
        width: z.number(),
        height: z.number(),
        radius: z.number(),
      }),
      // Body-derived faces carry per-face triangulation; legacy
      // analytical faces leave these empty and rely on (size, plane_frame)
      // for the UI's pick mesh.
      triangle_positions: z.array(z.number()).default([]),
      triangle_indices: z.array(z.number()).default([]),
      is_selected: z.boolean(),
      appearance_color: z.string().nullable().default(null),
    }),
  ),
  reference_planes: z.array(
    z.object({
      reference_id: z.string(),
      label: z.string(),
      // Origin planes use one of "xy" / "yz" / "xz"; construction
      // planes use "custom" and ship a real `plane_frame`.
      orientation: z.enum(["xy", "yz", "xz", "custom"]),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      size: z.object({
        width: z.number(),
        height: z.number(),
      }),
      is_selected: z.boolean(),
      is_active_sketch_plane: z.boolean(),
      // Defaulted to null so older snapshots without the field
      // (origin-only planes from a pre-construction-plane core)
      // still validate.
      plane_frame: z
        .object({
          origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable()
        .default(null),
    }),
  ),
  reference_axes: z.array(
    z.object({
      reference_id: z.string(),
      label: z.string(),
      axis: z.enum(["x", "y", "z", "custom"]),
      start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
    }),
  ),
  reference_points: z
    .array(
      z.object({
        reference_id: z.string(),
        label: z.string(),
        position: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  helices: z
    .array(
      z.object({
        helix_id: z.string(),
        label: z.string(),
        points: z.array(z.number()).default([]),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  sketch_lines: z.array(
    z.object({
      line_id: z.string(),
      start_vertex_id: z.string(),
      end_vertex_id: z.string(),
      is_construction: z.boolean().default(false),
      plane_id: z.string(),
      start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_selected: z.boolean(),
      constraint: z.enum(["horizontal", "vertical"]).nullable(),
      // True for transient lines generated by the in-progress
      // Mirror tool. Rendered as a dashed translucent preview;
      // not selectable. Defaulted for back-compat with the few
      // call sites that may not yet emit it.
      is_preview: z.boolean().default(false),
      // Set for glyph segments expanded from a sketch text entry
      // ("text:<text-id>"). Defaulted to null so older cores keep
      // emitting valid snapshots.
      generated_by: z.string().nullable().default(null),
    }),
  ),
  sketch_circles: z.array(
    z.object({
      circle_id: z.string(),
      plane_id: z.string(),
      plane_frame: planeFrameSchema.nullable().default(null),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      radius: z.number(),
      is_selected: z.boolean(),
      is_construction: z.boolean().default(false),
      // See `sketch_lines.is_preview`.
      is_preview: z.boolean().default(false),
      // See `sketch_lines.generated_by` (reserved — the v1 text
      // expansion only emits lines).
      generated_by: z.string().nullable().default(null),
    }),
  ),
  // Sketch ellipses — center + major/minor radii with the major-axis
  // rotation in sketch-plane coordinates. Defaulted to `[]` so clients
  // running against an older core don't crash.
  sketch_ellipses: z
    .array(
      z.object({
        ellipse_id: z.string(),
        plane_id: z.string(),
        plane_frame: planeFrameSchema.nullable().default(null),
        center: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        a: z.number(),
        b: z.number(),
        rotation: z.number(),
        is_selected: z.boolean(),
        is_construction: z.boolean().default(false),
        // See `sketch_lines.is_preview`.
        is_preview: z.boolean().default(false),
        generated_by: z.string().nullable().default(null),
      }),
    )
    .default([]),
  // Control-point B-splines — the curve as a sampled world polyline
  // plus the control poles. Defaulted to `[]` so clients running
  // against an older core don't crash.
  sketch_splines: z
    .array(
      z.object({
        spline_id: z.string(),
        plane_id: z.string(),
        plane_frame: planeFrameSchema.nullable().default(null),
        curve_points: z.array(
          z.object({
            x: z.number(),
            y: z.number(),
            z: z.number(),
          }),
        ),
        pole_points: z.array(
          z.object({
            x: z.number(),
            y: z.number(),
            z: z.number(),
          }),
        ),
        degree: z.number().default(3),
        is_selected: z.boolean(),
        is_construction: z.boolean().default(false),
        // See `sketch_lines.is_preview`.
        is_preview: z.boolean().default(false),
        generated_by: z.string().nullable().default(null),
      }),
    )
    .default([]),
  // Sketch polygons — regular N-sided polygons on the sketch plane.
  // Defaulted to `[]` so clients running against an older core
  // don't crash.
  sketch_polygons: z
    .array(
      z.object({
        polygon_id: z.string(),
        plane_id: z.string(),
        plane_frame: planeFrameSchema.nullable().default(null),
        corner_x: z.array(z.number()),
        corner_y: z.array(z.number()),
        corner_z: z.array(z.number()),
        sides: z.number(),
        mode: z.string(),
        center: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        radius: z.number(),
        is_selected: z.boolean(),
        is_construction: z.boolean().default(false),
        is_preview: z.boolean().default(false),
      }),
    )
    .default([]),
  // Sketch arcs share the same visual treatment as sketch circles
  // (selectable, preview-aware) but carry both endpoints + center
  // separately because the renderer samples between the two
  // endpoints around the center along the stored sweep direction.
  // Defaulted to `[]` so clients running against an older core
  // (no `sketch_arcs` key in the payload) don't crash on the
  // downstream `.filter()` call in `viewportScene.ts`.
  sketch_arcs: z
    .array(
      z.object({
        arc_id: z.string(),
        start_vertex_id: z.string(),
        end_vertex_id: z.string(),
        plane_id: z.string(),
        plane_frame: planeFrameSchema.nullable().default(null),
        center: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        radius: z.number(),
        start: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        end: z.object({
          x: z.number(),
          y: z.number(),
          z: z.number(),
        }),
        ccw: z.boolean(),
        is_selected: z.boolean(),
        is_construction: z.boolean().default(false),
        is_preview: z.boolean().default(false),
        // See `sketch_lines.generated_by` (reserved — the v1 text
        // expansion only emits lines).
        generated_by: z.string().nullable().default(null),
      }),
    )
    .default([]),
  sketch_vertices: z.array(
    z.object({
      vertex_id: z.string(),
      plane_id: z.string(),
      kind: z.enum(["endpoint", "center", "projected", "quadrant", "fillet_corner"]),
      position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_fixed: z.boolean(),
      is_selected: z.boolean(),
      // ── Vertex unification (Phase 1) ───────────────────────
      geometry_owner_ids: z.array(z.string()).optional().default([]),
      is_projected: z.boolean().optional().default(false),
      source_type: z.string().optional(),
      source_feature_id: z.string().optional(),
      source_edge_id: z.string().optional(),
    }).passthrough(),
  ),
  sketch_dimensions: z.array(
    z.object({
      dimension_id: z.string(),
      plane_id: z.string(),
      kind: z.enum([
        "line_length",
        "circle_radius",
        "arc_radius",
        "polygon_radius",
        "angle",
        "line_angle",
        "line_line_distance",
        "circle_center_distance",
        "circle_line_distance",
        "point_distance",
      ]),
      entity_id: z.string(),
      label: z.string(),
      is_selected: z.boolean(),
      anchor_start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      anchor_end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      dimension_start: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      dimension_end: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      label_position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      // Angle arc geometry (from C++ core, optional)
      arc_center: z
        .object({ x: z.number(), y: z.number(), z: z.number() })
        .optional(),
      arc_radius: z.number().optional(),
      arc_start_angle: z.number().optional(),
      arc_end_angle: z.number().optional(),
      arc_ccw: z.boolean().optional(),
      // Reference line (from C++ core, optional)
      ref_line_start: z
        .object({ x: z.number(), y: z.number(), z: z.number() })
        .optional(),
      ref_line_end: z
        .object({ x: z.number(), y: z.number(), z: z.number() })
        .optional(),
      driven: z.boolean().default(false),
      display_as: z.string().default(""),
    }),
  ),
  sketch_constraints: z.array(
    z.object({
      constraint_id: z.string(),
      plane_id: z.string(),
      kind: z.enum([
        "horizontal",
        "vertical",
        "equal_length",
        "perpendicular",
        "parallel",
        "fixed",
        "midpoint",
        "on_line",
        "tangent_line_circle",
        "tangent_circle_line",
        "coincident",
        "concentric",
        "quadrant",
        "mirror",
      ]),
      entity_id: z.string(),
      related_entity_id: z.string().nullable(),
      label: z.string(),
      is_selected: z.boolean(),
      position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      driven: z.boolean().default(false),
    }),
  ),
  sketch_profiles: z.array(
    z.object({
      profile_id: z.string(),
      plane_id: z.string(),
      plane_frame: z
        .object({
          origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        })
        .nullable(),
      profile_kind: z.enum(["polygon", "circle"]),
      profile_points: z.array(
        z.object({
          x: z.number(),
          y: z.number(),
        }),
      ),
      start_x: z.number(),
      start_y: z.number(),
      width: z.number(),
      height: z.number(),
      radius: z.number(),
      is_selected: z.boolean(),
    }),
  ),
  meshes: z
    .array(
      z.object({
        primitive_id: z.string(),
        positions: z.array(z.number()),
        normals: z.array(z.number()),
        indices: z.array(z.number()),
        is_selected: z.boolean(),
        appearance_color: z.string().nullable().default(null),
      }),
    )
    .default([]),
  cut_previews: z
    .array(
      z.object({
        id: z.string(),
        positions: z.array(z.number()),
        normals: z.array(z.number()),
        indices: z.array(z.number()),
      }),
    )
    .default([]),
  toolpaths: z
    .array(
      z.object({
        id: z.string(),
        label: z.string(),
        points: z.array(
          z.object({
            x: z.number(),
            y: z.number(),
            z: z.number(),
            is_rapid: z.boolean(),
          }),
        ),
      }),
    )
    .default([]),
  bodies: z
    .array(
      z.object({
        id: z.string(),
        label: z.string(),
        center: z
          .object({ x: z.number(), y: z.number(), z: z.number() })
          .default({ x: 0, y: 0, z: 0 }),
        size: z
          .object({ x: z.number(), y: z.number(), z: z.number() })
          .default({ x: 0, y: 0, z: 0 }),
        local_frame: z
          .object({
            x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            z_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
          })
          .default({
            x_axis: { x: 1, y: 0, z: 0 },
            y_axis: { x: 0, y: 1, z: 0 },
            z_axis: { x: 0, y: 0, z: 1 },
          }),
      }),
    )
    .default([]),
  edges: z
    .array(
      z.object({
        id: z.string(),
        owner_body_id: z.string(),
        kind: z.string(),
        points: z.array(z.number()),
        // Default 0 so older snapshots without the field still validate;
        // new core builds always populate it.
        length: z.number().default(0),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  vertices: z
    .array(
      z.object({
        id: z.string(),
        owner_body_id: z.string(),
        position: z.object({ x: z.number(), y: z.number(), z: z.number() }),
        is_selected: z.boolean(),
      }),
    )
    .default([]),
  scene_width: z.number(),
  scene_height: z.number(),
  scene_depth: z.number(),
  scene_bounds: z.object({
    center: z.object({
      x: z.number(),
      y: z.number(),
      z: z.number(),
    }),
    size: z.object({
      x: z.number(),
      y: z.number(),
      z: z.number(),
    }),
    max_dimension: z.number(),
  }),
  dof_statuses: z.array(
    z.object({
      entity_id: z.string(),
      entity_kind: z.string(),
      total_dof: z.number(),
      consumed_dof: z.number(),
      status: z.enum(["under", "full", "over"]),
    }),
  ),
  solver_dofs: z.number().optional().default(-1),
  solver_conflicting_count: z.number().optional().default(-1),
  solver_redundant_count: z.number().optional().default(-1),
  snap_candidates: z.array(
    z.object({
      kind: z.string(),
      entity_id: z.string(),
      vertex_id: z.string(),
      local_x: z.number(),
      local_y: z.number(),
      label: z.string(),
    }),
  ),
  selection_filter: z.object({
    select_curves: z.boolean(),
    select_points: z.boolean(),
    select_construction: z.boolean(),
    select_constraints: z.boolean(),
    snap_endpoint: z.boolean(),
    snap_midpoint: z.boolean(),
    snap_center: z.boolean(),
    snap_intersection: z.boolean(),
    snap_nearest: z.boolean(),
    snap_circle_body: z.boolean().optional().default(true),
    snap_arc_body: z.boolean().optional().default(true),
    snap_quadrant: z.boolean(),
    snap_perpendicular: z.boolean(),
    snap_parallel: z.boolean(),
    snap_tangent: z.boolean(),
    snap_grid: z.boolean(),
    snap_grid_line: z.boolean(),
    snap_polar: z.boolean(),
    polar_angle_degrees: z.number(),
    parallel_angle_degrees: z.number().default(8),
    magnetic_pull: z.boolean(),
    tolerance_px: z.number(),
  }),
});
