// CAM command payload contracts — mirroring the core's cam_* commands
// (native/cad-core/src/app/impl/cam_commands.inc).  Every mutator
// replies with a `document_state` event; errors reply with an `error`
// event `{code, message}`.

import type {
  CamOperation,
  CamOperationPayload,
  CamSetup,
  LaserMachineSettings,
  PostProcessor,
  StockDefinition,
  ToolEntry,
} from "../geometry/cam";

export interface CamMachineSettingsSetCommand {
  id: string;
  type: "cam_machine_settings_set";
  payload: LaserMachineSettings;
}

export interface CamCaptureFaceReferenceCommand {
  id: string;
  type: "cam_capture_face_reference";
  payload: { face_id: string };
}

export interface CamWcsSetFaceCommand {
  id: string;
  type: "cam_wcs_set_face";
  payload: { face_id: string };
}

export interface CamSetupCreateCommand {
  id: string;
  type: "cam_setup_create";
  payload: CamSetup;
}

export interface CamSetupUpdateCommand {
  id: string;
  type: "cam_setup_update";
  payload: CamSetup;
}

export interface CamSetupDeleteCommand {
  id: string;
  type: "cam_setup_delete";
  payload: { setup_id: string };
}

export interface CamSetupGetCommand {
  id: string;
  type: "cam_setup_get";
  payload: Record<string, never>;
}

export interface CamStockSetCommand {
  id: string;
  type: "cam_stock_set";
  payload: StockDefinition;
}

export interface CamStockGetCommand {
  id: string;
  type: "cam_stock_get";
  payload: Record<string, never>;
}

export interface CamToolAddCommand {
  id: string;
  type: "cam_tool_add";
  payload: Omit<ToolEntry, "tool_id"> & { tool_id?: string };
}

export interface CamToolUpdateCommand {
  id: string;
  type: "cam_tool_update";
  payload: ToolEntry;
}

export interface CamToolDeleteCommand {
  id: string;
  type: "cam_tool_delete";
  payload: { tool_id: string };
}

export interface CamToolListCommand {
  id: string;
  type: "cam_tool_list";
  payload: Record<string, never>;
}

export interface CamOperationCreateCommand {
  id: string;
  type: "cam_operation_create";
  payload: CamOperationPayload;
}

export interface CamOperationUpdateCommand {
  id: string;
  type: "cam_operation_update";
  payload: { op_id: string } & Partial<CamOperation>;
}

export interface CamOperationDeleteCommand {
  id: string;
  type: "cam_operation_delete";
  payload: { op_id: string };
}

export interface CamOperationSetScopeCommand {
  id: string;
  type: "cam_operation_set_scope";
  payload: { op_id: string; kind: "sketch"; feature_id: string };
}

export interface CamPostProcessorSetCommand {
  id: string;
  type: "cam_post_processor_set";
  payload: PostProcessor;
}

export interface CamPostListCommand {
  id: string;
  type: "cam_post_list";
  payload: Record<string, never>;
}

export interface CamPostImportCommand {
  id: string;
  type: "cam_post_import";
  payload: {
    source_path: string;
  };
}

export interface CamOperationGenerateCommand {
  id: string;
  type: "cam_operation_generate";
  payload: { op_id: string };
}

export interface CamOperationPreviewCommand {
  id: string;
  type: "cam_operation_preview";
  payload: { op_id: string };
}

export interface CamExportGcodeCommand {
  id: string;
  type: "cam_export_gcode";
  payload: { file_path: string };
}
