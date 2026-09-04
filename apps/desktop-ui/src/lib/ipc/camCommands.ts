import type {
  CamOperation,
  CamOperationPayload,
  CamSetup,
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
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_update",
    payload: { op_id: opId, ...partial },
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
