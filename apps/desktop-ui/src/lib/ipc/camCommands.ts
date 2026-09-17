import type {
  CamOperation,
  CamOperationPayload,
  CamSetup,
  CamToolImportMode,
  CoreCommand,
  LaserMachineSettings,
  MachineDefinition,
  PostProcessor,
  StockDefinition,
  ToolEntry,
} from "@/types";

export function makeCamMachineSettingsSetCommand(
  machineSettings: LaserMachineSettings,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_machine_settings_set",
    payload: machineSettings,
  };
}

export function makeCamCaptureFaceReferenceCommand(
  faceId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_capture_face_reference",
    payload: { face_id: faceId },
  };
}

export function makeCamCaptureEdgeReferenceCommand(
  edgeId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_capture_edge_reference",
    payload: { edge_id: edgeId },
  };
}

// Drilling hole locations: the UI reports the clicked world point, the
// core mints the persistent reference id and replies with a
// cam_attestation_result event.
export function makeCamCapturePointCommand(point: {
  x: number;
  y: number;
  z: number;
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_capture_point",
    payload: { x: point.x, y: point.y, z: point.z },
  };
}

export function makeCamWcsSetFaceCommand(
  faceId: string,
  setupId?: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_wcs_set_face",
    payload: { face_id: faceId, setup_id: setupId },
  };
}

// CAM command factories — every command replies with a `document_state`
// event (errors reply with an `error` event). Payloads are the
// serialized target-schema structs from types/geometry/cam.ts.

export function makeCamSetupCreateCommand(camSetup: CamSetup): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_create",
    payload: camSetup,
  };
}

export function makeCamSetupUpdateCommand(camSetup: CamSetup): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_update",
    payload: camSetup,
  };
}

export function makeCamSetupDeleteCommand(setupId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_delete",
    payload: { setup_id: setupId },
  };
}

export function makeCamSetupGetCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_get",
    payload: {},
  };
}

export function makeCamStockSetCommand(stock: StockDefinition): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_stock_set",
    payload: stock,
  };
}

export function makeCamStockGetCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_stock_get",
    payload: {},
  };
}

export function makeCamToolAddCommand(
  tool: Omit<ToolEntry, "tool_id"> & { tool_id?: string },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_add",
    payload: tool,
  };
}

export function makeCamToolUpdateCommand(
  toolId: string,
  tool: ToolEntry,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_update",
    payload: { ...tool, tool_id: toolId },
  };
}

export function makeCamToolDeleteCommand(toolId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_delete",
    payload: { tool_id: toolId },
  };
}

export function makeCamToolListCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_list",
    payload: {},
  };
}

export function makeCamToolLibraryListCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_library_list",
    payload: {},
  };
}

export function makeCamToolLibrarySaveCommand(tool: ToolEntry): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_library_save",
    payload: tool,
  };
}

export function makeCamToolParseTextCommand(
  format: "linuxcnc_tbl" | "polysmith_json",
  text: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_parse_text",
    payload: { format, text },
  };
}

export function makeCamToolExportTextCommand(
  format: "linuxcnc_tbl" | "polysmith_json",
  toolNumbers?: number[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_export_text",
    payload: { format, tool_numbers: toolNumbers },
  };
}

export function makeCamToolParseFileCommand(sourcePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_parse_file",
    payload: { source_path: sourcePath },
  };
}

export function makeCamToolImportFileCommand(
  sourcePath: string,
  mode: CamToolImportMode,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_import_file",
    payload: { source_path: sourcePath, mode },
  };
}

export function makeCamToolExportFileCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_tool_export_file",
    payload: { file_path: filePath },
  };
}

export function makeCamOperationCreateCommand(
  operation: CamOperationPayload,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_create",
    payload: operation,
  };
}

export function makeCamOperationUpdateCommand(
  opId: string,
  partial: Partial<CamOperation>,
  options?: {
    // Carried alongside an emptied geometry_references: the explicit
    // profile ids the core must re-capture (the panel's Apply).  With
    // them the capture does not depend on the live document selection
    // still being present when the command is processed.
    selected_profile_ids?: string[];
  },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_update",
    payload: { op_id: opId, ...partial, ...(options ?? {}) },
  };
}

export function makeCamOperationDeleteCommand(opId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_delete",
    payload: { op_id: opId },
  };
}

export function makeCamOperationSetScopeCommand(
  opId: string,
  featureId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_set_scope",
    payload: { op_id: opId, kind: "sketch", feature_id: featureId },
  };
}

export function makeCamPostProcessorSetCommand(
  post: PostProcessor,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_post_processor_set",
    payload: post,
  };
}

export function makeCamPostListCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_post_list",
    payload: {},
  };
}

export function makeCamPostImportCommand(sourcePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_post_import",
    payload: { source_path: sourcePath },
  };
}

// Machine library commands — cam_machine_list replies with a
// cam_machine_list_result event (not document_state); cam_machine_save
// validates + writes the definition and replies the refreshed library.
export function makeCamMachineListCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_machine_list",
    payload: {},
  };
}

export function makeCamMachineSaveCommand(
  machine: MachineDefinition,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_machine_save",
    payload: machine,
  };
}

export function makeCamOperationGenerateCommand(opId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_generate",
    payload: { op_id: opId },
  };
}

export function makeCamOperationPreviewCommand(opId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_preview",
    payload: { op_id: opId },
  };
}

export function makeCamExportGcodeCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_export_gcode",
    payload: { file_path: filePath },
  };
}

// In-memory posting for the GRBL workspace handoff — same pipeline as
// cam_export_gcode, but the reply carries the program text.
export function makeCamExportGcodeTextCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_export_gcode_text",
    payload: {},
  };
}
