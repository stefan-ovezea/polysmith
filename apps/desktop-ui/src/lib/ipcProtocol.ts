import { z } from "zod";
import type {
  CoreCommand,
  CoreMessage,
  DocumentState,
  DocumentExportResult,
  ErrorEvent,
  ViewportState,
} from "../types/ipc";

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
      kind: z.enum(["horizontal", "vertical", "equal_length", "perpendicular", "parallel"]),
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

const coreMessageSchema = z.union([
  helloEventSchema,
  pongEventSchema,
  documentCreatedEventSchema,
  documentStateEventSchema,
  sessionStateEventSchema,
  viewportStateEventSchema,
  documentExportedEventSchema,
  errorEventSchema,
]);

export function parseCoreMessage(input: unknown): CoreMessage {
  return coreMessageSchema.parse(input) as CoreMessage;
}

export function makePingCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "ping",
    payload: {},
  };
}

export function makeCreateDocumentCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_document",
    payload: {},
  };
}

export function makeGetDocumentStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_document_state",
    payload: {},
  };
}

export function makeGetSessionStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_session_state",
    payload: {},
  };
}

export function makeGetViewportStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_viewport_state",
    payload: {},
  };
}

export function makeExportDocumentCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_document",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeAddBoxFeatureCommand(
  width: number,
  height: number,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_box_feature",
    payload: {
      width,
      height,
      depth,
    },
  };
}

export function makeAddCylinderFeatureCommand(
  radius: number,
  height: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_cylinder_feature",
    payload: {
      radius,
      height,
    },
  };
}

export function makeUpdateBoxFeatureCommand(
  featureId: string,
  width: number,
  height: number,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_box_feature",
    payload: {
      feature_id: featureId,
      width,
      height,
      depth,
    },
  };
}

export function makeRenameFeatureCommand(
  featureId: string,
  name: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "rename_feature",
    payload: {
      feature_id: featureId,
      name,
    },
  };
}

export function makeDeleteFeatureCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_feature",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeUndoCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "undo",
    payload: {},
  };
}

export function makeRedoCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "redo",
    payload: {},
  };
}

export function makeSelectFeatureCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_feature",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeSelectReferenceCommand(referenceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_reference",
    payload: {
      reference_id: referenceId,
    },
  };
}

export function makeSelectFaceCommand(faceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_face",
    payload: {
      face_id: faceId,
    },
  };
}

export function makeStartSketchOnPlaneCommand(referenceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "start_sketch_on_plane",
    payload: {
      reference_id: referenceId,
    },
  };
}

export function makeStartSketchOnFaceCommand(
  faceId: string,
  planeFrame: {
    origin: { x: number; y: number; z: number };
    x_axis: { x: number; y: number; z: number };
    y_axis: { x: number; y: number; z: number };
    normal: { x: number; y: number; z: number };
  },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "start_sketch_on_face",
    payload: {
      face_id: faceId,
      plane_frame: planeFrame,
    },
  };
}

export function makeSetSketchToolCommand(
  tool: "select" | "line" | "rectangle" | "circle",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_tool",
    payload: {
      tool,
    },
  };
}

export function makeUpdateSketchLineCommand(
  lineId: string,
  startX: number,
  startY: number,
  endX: number,
  endY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_line",
    payload: {
      line_id: lineId,
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
    },
  };
}

export function makeSetSketchLineConstraintCommand(
  lineId: string,
  constraint: "none" | "horizontal" | "vertical",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_line_constraint",
    payload: {
      line_id: lineId,
      constraint,
    },
  };
}

export function makeSetSketchEqualLengthConstraintCommand(
  lineId: string,
  otherLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_equal_length_constraint",
    payload: {
      line_id: lineId,
      other_line_id: otherLineId ?? "none",
    },
  };
}

export function makeSetSketchPerpendicularConstraintCommand(
  lineId: string,
  otherLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_perpendicular_constraint",
    payload: {
      line_id: lineId,
      other_line_id: otherLineId ?? "none",
    },
  };
}

export function makeSetSketchParallelConstraintCommand(
  lineId: string,
  otherLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_parallel_constraint",
    payload: {
      line_id: lineId,
      other_line_id: otherLineId ?? "none",
    },
  };
}

export function makeSetSketchCoincidentConstraintCommand(
  pointId: string,
  otherPointId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_coincident_constraint",
    payload: {
      point_id: pointId,
      other_point_id: otherPointId,
    },
  };
}

export function makeUpdateSketchCircleCommand(
  circleId: string,
  centerX: number,
  centerY: number,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_circle",
    payload: {
      circle_id: circleId,
      center_x: centerX,
      center_y: centerY,
      radius,
    },
  };
}

export function makeUpdateSketchDimensionCommand(
  dimensionId: string,
  value: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_dimension",
    payload: {
      dimension_id: dimensionId,
      value,
    },
  };
}

export function makeSelectSketchProfileCommand(profileId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_profile",
    payload: {
      profile_id: profileId,
    },
  };
}

export function makeExtrudeProfileCommand(
  profileId: string,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "extrude_profile",
    payload: {
      profile_id: profileId,
      depth,
    },
  };
}

export function makeAddSketchLineCommand(
  startX: number,
  startY: number,
  endX: number,
  endY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_line",
    payload: {
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
    },
  };
}

export function makeAddSketchRectangleCommand(
  startX: number,
  startY: number,
  endX: number,
  endY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_rectangle",
    payload: {
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
    },
  };
}

export function makeAddSketchCircleCommand(
  centerX: number,
  centerY: number,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_circle",
    payload: {
      center_x: centerX,
      center_y: centerY,
      radius,
    },
  };
}

export function makeSelectSketchEntityCommand(entityId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_entity",
    payload: {
      entity_id: entityId,
    },
  };
}

export function makeSelectSketchDimensionCommand(
  dimensionId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_dimension",
    payload: {
      dimension_id: dimensionId,
    },
  };
}

export function makeFinishSketchCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "finish_sketch",
    payload: {},
  };
}

export function makeClearSelectionCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_selection",
    payload: {},
  };
}

export function getDocumentFromMessage(message: CoreMessage): DocumentState | null {
  if (message.type === "document_created" || message.type === "document_state") {
    return message.payload;
  }

  return null;
}

export function getErrorFromMessage(message: CoreMessage): ErrorEvent | null {
  if (message.type === "error") {
    return message;
  }

  return null;
}

export function getViewportFromMessage(message: CoreMessage): ViewportState | null {
  if (message.type === "viewport_state") {
    return message.payload;
  }

  return null;
}

export function getDocumentExportFromMessage(
  message: CoreMessage,
): DocumentExportResult | null {
  if (message.type === "document_exported") {
    return message.payload;
  }

  return null;
}
