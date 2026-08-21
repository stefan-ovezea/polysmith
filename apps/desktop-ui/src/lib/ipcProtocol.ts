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
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "trim_preview",
    payload: {
      entity_id: entityId,
      cursor_x: cursorX,
      cursor_y: cursorY,
    },
  };
}

// ── CAM ───────────────────────────────────────────────────────────

export function makeCamSetupCreateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_create",
    payload: {
      name: "Setup",
      machine_config: { machine_type: "3_axis" },
      stock: {
        width: 120,
        height: 120,
        depth: 20,
        offset_x: 5,
        offset_y: 5,
        offset_z: 5,
      },
      wcs_origin: { x: 0, y: 0, z: 0 },
    },
  } as unknown as CoreCommand;
}

// Legacy CAM types used by the setup panel (being rebuilt on cam_types.h schema).
interface CamSetupStock {
  width: number; height: number; depth: number;
  offset_x: number; offset_y: number; offset_z: number;
}
interface CamSetupOrigin { x: number; y: number; z: number; }

export interface CamSetupUpdatePayload {
  stock?: CamSetupStock;
  wcs_origin?: CamSetupOrigin;
  safety_plane_z?: number;
  wcs_angle?: number;
}

export function makeCamSetupUpdateCommand(payload: CamSetupUpdatePayload): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_update",
    payload,
  } as unknown as CoreCommand;
}

export function makeCamOperationCreateCommand(
  operationType: string,
  bodyId: string,
  faceIndex: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_create",
    payload: {
      operation_type: operationType,
      name: operationType === "face_milling" ? "Face Mill" : operationType,
      tool_id: "endmill_6mm",
      face_reference: { body_id: bodyId, face_index: faceIndex },
      params: { depth: 0.5, stepover: 5, angle_deg: 0 },
    },
  } as unknown as CoreCommand;
}

export interface CamOperationUpdatePayload {
  operation_id: string;
  name?: string;
  tool_id?: string;
  params?: { depth: number; stepover: number; angle_deg: number };
}

export function makeCamOperationUpdateCommand(payload: CamOperationUpdatePayload): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_update",
    payload,
  } as unknown as CoreCommand;
}

export function makeCamOperationDeleteCommand(operationId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_delete",
    payload: { operation_id: operationId },
  } as unknown as CoreCommand;
}
