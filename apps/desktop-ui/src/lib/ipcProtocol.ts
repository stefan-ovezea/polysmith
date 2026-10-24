import type {
  CoreCommand,
  CoreMessage,
  DocumentState,
  DocumentExportResult,
  ErrorEvent,
  SelectionFilterUpdate,
  ViewportState,
} from "@/types";

import { coreMessageSchema } from "./schemas/ipcSchema";

export * from "./ipc/sketchCommands";
export * from "./ipc/profileFeatureCommands";
export * from "./ipc/bodyFeatureCommands";
export * from "./ipc/camCommands";
export * from "./ipc/drawingCommands";

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

export function makeGetSketchProfilesCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_sketch_profiles",
    payload: {
      feature_id: featureId,
    },
  };
}

export interface PreviewAutoConstraintsOptions {
  featureId?: string;
  tolerance?: number;
  angleTolerance?: number;
}

export function makeAutoConstrainSketchCommand(
  options: { tolerance?: number; angleTolerance?: number } = {},
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "auto_constrain_sketch",
    payload: {
      ...(options.tolerance !== undefined ? { tolerance: options.tolerance } : {}),
      ...(options.angleTolerance !== undefined
        ? { angle_tolerance: options.angleTolerance }
        : {}),
    },
  };
}

export function makePreviewAutoConstraintsCommand(
  options: PreviewAutoConstraintsOptions = {},
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "preview_auto_constraints",
    payload: {
      ...(options.featureId ? { feature_id: options.featureId } : {}),
      ...(options.tolerance !== undefined ? { tolerance: options.tolerance } : {}),
      ...(options.angleTolerance !== undefined
        ? { angle_tolerance: options.angleTolerance }
        : {}),
    },
  };
}

export function makeGetSketchDiagnosticsCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_sketch_diagnostics",
    payload: {
      feature_id: featureId,
    },
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

export function makeExportDocumentStlCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_document_stl",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeExportBodyStlCommand(
  filePath: string,
  bodyId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_body_stl",
    payload: {
      file_path: filePath,
      body_id: bodyId,
    },
  };
}

export function makeExportBodyStepCommand(
  filePath: string,
  bodyId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_body_step",
    payload: {
      file_path: filePath,
      body_id: bodyId,
    },
  };
}

export function makeSaveDocumentCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "save_document",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeLoadDocumentCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "load_document",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeImportStlCommand(
  filePath: string,
  scale = 1.0,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "import_stl",
    payload: {
      file_path: filePath,
      scale,
    },
  };
}

export function makeImportDxfCommand(
  filePath: string,
  planeId?: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "import_dxf",
    payload: planeId
      ? {
          file_path: filePath,
          plane_id: planeId,
        }
      : {
          file_path: filePath,
        },
  };
}

export function makeImportStepCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "import_step",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeImportIgesCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "import_iges",
    payload: {
      file_path: filePath,
    },
  };
}

// ── PCB fabrication layers ─────────────────────────────────────────
// The core resolves each layer's role from the file itself; `role` is only
// sent when the layer table overrides it.

export function makePcbImportLayerCommand(
  filePath: string,
  role?: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "pcb_import_layer",
    payload: role ? { file_path: filePath, role } : { file_path: filePath },
  };
}

export function makePcbUpdateLayerCommand(
  featureId: string,
  options: { role?: string; mirrorX?: boolean } = {},
): CoreCommand {
  const payload: { feature_id: string; role?: string; mirror_x?: boolean } = {
    feature_id: featureId,
  };
  if (options.role !== undefined) {
    payload.role = options.role;
  }
  if (options.mirrorX !== undefined) {
    payload.mirror_x = options.mirrorX;
  }
  return { id: crypto.randomUUID(), type: "pcb_update_layer", payload };
}

export function makePcbRemoveLayerCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "pcb_remove_layer",
    payload: { feature_id: featureId },
  };
}

export function makePcbCaptureLayerReferenceCommand(
  featureId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "pcb_capture_layer_reference",
    payload: { feature_id: featureId },
  };
}

export function makeExportDocumentDxfCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_document_dxf",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeExportDocumentIgesCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_document_iges",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeConvertMeshToBodyCommand(bodyId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "convert_mesh_to_body",
    payload: {
      body_id: bodyId,
    },
  };
}

export function makeDetachBodyProjectionsCommand(
  bodyId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "detach_body_projections",
    payload: {
      body_id: bodyId,
    },
  };
}

export function makeRemoveSketchProjectionsCommand(
  featureId: string,
  keepGeometry: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "remove_sketch_projections",
    payload: {
      feature_id: featureId,
      keep_geometry: keepGeometry,
    },
  };
}

export function makeRedefineSketchPlaneCommand(
  featureId: string,
  planeId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "redefine_sketch_plane",
    payload: {
      feature_id: featureId,
      plane_id: planeId,
    },
  };
}

export function makeProjectBodyIntoSketchCommand(
  bodyId: string,
  mode: "section" | "silhouette",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_body_into_sketch",
    payload: {
      body_id: bodyId,
      mode,
    },
  };
}

export function makeProjectFaceIntoSketchCommand(faceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_face_into_sketch",
    payload: {
      face_id: faceId,
    },
  };
}

export function makeProjectProfileIntoSketchCommand(
  profileId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_profile_into_sketch",
    payload: {
      profile_id: profileId,
    },
  };
}

export function makeProjectEdgeIntoSketchCommand(edgeId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_edge_into_sketch",
    payload: {
      edge_id: edgeId,
    },
  };
}

export function makeProjectVertexIntoSketchCommand(
  vertexId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_vertex_into_sketch",
    payload: {
      vertex_id: vertexId,
    },
  };
}

export function makeUpdateSelectionFilterCommand(
  filter: SelectionFilterUpdate,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_selection_filter",
    payload: { ...filter },
  };
}

export function makeAddParameterCommand(
  name: string,
  expression: string,
  kind: "length" | "angle" = "length",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_parameter",
    payload: { name, expression, kind },
  };
}

export function makeUpdateParameterCommand(
  name: string,
  expression: string,
  kind: "length" | "angle" = "length",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_parameter",
    payload: { name, expression, kind },
  };
}

export function makeDeleteParameterCommand(
  name: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_parameter",
    payload: { name },
  };
}
export function makeClearSelectionCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_selection",
    payload: {},
  };
}

export function getDocumentFromMessage(
  message: CoreMessage,
): DocumentState | null {
  if (
    message.type === "document_created" ||
    message.type === "document_state"
  ) {
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

export function getViewportFromMessage(
  message: CoreMessage,
): ViewportState | null {
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

export function makeTrimPreviewCommand(
  entityId: string,
  cursorX: number,
  cursorY: number,
  requestId?: string,
): CoreCommand {
  return {
    id: requestId ?? crypto.randomUUID(),
    type: "trim_preview",
    payload: {
      entity_id: entityId,
      cursor_x: cursorX,
      cursor_y: cursorY,
    },
  };
}

export function makeCornerTrimPreviewCommand(
  entityAId: string,
  entityBId: string,
  cursorX: number,
  cursorY: number,
  requestId?: string,
): CoreCommand {
  return {
    id: requestId ?? crypto.randomUUID(),
    type: "corner_trim_preview",
    payload: {
      entity_a_id: entityAId,
      entity_b_id: entityBId,
      cursor_x: cursorX,
      cursor_y: cursorY,
    },
  };
}
