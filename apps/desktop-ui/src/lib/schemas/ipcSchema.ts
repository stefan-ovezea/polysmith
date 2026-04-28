import { z } from "zod";

const documentStateSchema = z.object({
  document_id: z.string(),
  name: z.string(),
  units: z.string(),
  revision: z.number(),
  selected_feature_id: z.string().nullable(),
  selected_reference_id: z.string().nullable(),
  selected_face_id: z.string().nullable(),
  active_sketch_plane_id: z.string().nullable(),
  active_sketch_face_id: z.string().nullable(),
  active_sketch_feature_id: z.string().nullable(),
  active_sketch_tool: z
    .enum(["select", "line", "rectangle", "circle"])
    .nullable(),
  selected_sketch_point_id: z.string().nullable(),
  selected_sketch_entity_id: z.string().nullable(),
  selected_sketch_dimension_id: z.string().nullable(),
  selected_sketch_profile_id: z.string().nullable(),
  feature_history: z.array(
    z.object({
      feature_id: z.string(),
      kind: z.string(),
      name: z.string(),
      status: z.string(),
      parameters_summary: z.string(),
      box_parameters: z
        .object({
          width: z.number(),
          height: z.number(),
          depth: z.number(),
        })
        .nullable(),
      cylinder_parameters: z
        .object({
          radius: z.number(),
          height: z.number(),
        })
        .nullable(),
      extrude_parameters: z
        .object({
          sketch_feature_id: z.string(),
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
          profile_kind: z.enum(["rectangle", "circle", "polygon"]),
          start_x: z.number(),
          start_y: z.number(),
          width: z.number(),
          height: z.number(),
          radius: z.number(),
          profile_points: z.array(
            z.object({
              x: z.number(),
              y: z.number(),
            }),
          ),
          depth: z.number(),
        })
        .nullable(),
      sketch_parameters: z
        .object({
          plane_id: z.string(),
          plane_frame: z
            .object({
              origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
              normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
            })
            .nullable(),
          active_tool: z.enum(["select", "line", "rectangle", "circle"]),
          lines: z.array(
            z.object({
              line_id: z.string(),
              start_point_id: z.string(),
              end_point_id: z.string(),
              start_x: z.number(),
              start_y: z.number(),
              end_x: z.number(),
              end_y: z.number(),
              constraint: z.enum(["horizontal", "vertical"]).nullable(),
            }),
          ),
          circles: z.array(
            z.object({
              circle_id: z.string(),
              center_x: z.number(),
              center_y: z.number(),
              radius: z.number(),
            }),
          ),
          points: z.array(
            z.object({
              point_id: z.string(),
              kind: z.enum(["endpoint", "center"]),
              x: z.number(),
              y: z.number(),
              is_fixed: z.boolean(),
            }),
          ),
          dimensions: z.array(
            z.object({
              dimension_id: z.string(),
              kind: z.enum(["line_length", "circle_radius"]),
              entity_id: z.string(),
              value: z.number(),
            }),
          ),
          line_relations: z.array(
            z.object({
              relation_id: z.string(),
              kind: z.enum(["equal_length", "perpendicular", "parallel"]),
              first_line_id: z.string(),
              second_line_id: z.string(),
            }),
          ),
          profiles: z.array(
            z.object({
              profile_id: z.string(),
              kind: z.enum(["polygon", "circle"]),
              point_ids: z.array(z.string()),
              line_ids: z.array(z.string()),
              points: z.array(
                z.object({
                  x: z.number(),
                  y: z.number(),
                }),
              ),
              source_circle_id: z.string().nullable(),
              center_x: z.number(),
              center_y: z.number(),
              radius: z.number(),
            }),
          ),
        })
        .nullable(),
    }),
  ),
});

const sessionStateSchema = z.object({
  document_count: z.number(),
  has_active_document: z.boolean(),
  active_document_id: z.string().nullable(),
  can_undo: z.boolean(),
  can_redo: z.boolean(),
});

const viewportStateSchema = z.object({
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
      depth: z.number(),
      is_selected: z.boolean(),
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
      is_selected: z.boolean(),
    }),
  ),
  reference_planes: z.array(
    z.object({
      reference_id: z.string(),
      label: z.string(),
      orientation: z.enum(["xy", "yz", "xz"]),
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
    }),
  ),
  reference_axes: z.array(
    z.object({
      reference_id: z.string(),
      label: z.string(),
      axis: z.enum(["x", "y", "z"]),
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
  sketch_lines: z.array(
    z.object({
      line_id: z.string(),
      start_point_id: z.string(),
      end_point_id: z.string(),
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
    }),
  ),
  sketch_circles: z.array(
    z.object({
      circle_id: z.string(),
      plane_id: z.string(),
      center: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      radius: z.number(),
      is_selected: z.boolean(),
    }),
  ),
  sketch_points: z.array(
    z.object({
      point_id: z.string(),
      plane_id: z.string(),
      kind: z.enum(["endpoint", "center"]),
      position: z.object({
        x: z.number(),
        y: z.number(),
        z: z.number(),
      }),
      is_fixed: z.boolean(),
      is_selected: z.boolean(),
    }),
  ),
  sketch_dimensions: z.array(
    z.object({
      dimension_id: z.string(),
      plane_id: z.string(),
      kind: z.enum(["line_length", "circle_radius"]),
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
});

const helloEventSchema = z.object({
  type: z.literal("hello"),
  payload: z.object({
    service: z.string(),
    version: z.string(),
  }),
});

const pongEventSchema = z.object({
  id: z.string(),
  type: z.literal("pong"),
  payload: z.object({
    version: z.string(),
  }),
});

const documentCreatedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_created"),
  payload: documentStateSchema,
});

const documentStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_state"),
  payload: documentStateSchema,
});

const sessionStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("session_state"),
  payload: sessionStateSchema,
});

const viewportStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("viewport_state"),
  payload: viewportStateSchema,
});

const documentExportedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_exported"),
  payload: z.object({
    file_path: z.string(),
    format: z.literal("step"),
    exported_feature_count: z.number(),
  }),
});

const errorEventSchema = z.object({
  id: z.string().optional(),
  type: z.literal("error"),
  payload: z.object({
    code: z.string(),
    message: z.string(),
  }),
});

export const coreMessageSchema = z.union([
  helloEventSchema,
  pongEventSchema,
  documentCreatedEventSchema,
  documentStateEventSchema,
  sessionStateEventSchema,
  viewportStateEventSchema,
  documentExportedEventSchema,
  errorEventSchema,
]);
