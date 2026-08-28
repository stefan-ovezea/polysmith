import type { CamOperation, CamSetup, LaserCutParameters } from "@/types";
import type { DocumentState, ViewportState } from "../types";
import {
  CamFaceMillingPanel,
  CamLaserCutPanel,
  CamSetupPanel,
  createDefaultCamSetup,
  type FaceMillingFormState,
} from "../layout";
import { DEFAULT_FACE_MILLING_PARAMS } from "../layout/CamFaceMillingPanel";
import { DEFAULT_LASER_PARAMS } from "../layout/CamLaserCutPanel";
import { awaitDocumentChange } from "../state/cadCoreStore";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface CamFloatingPanelsProps {
  document: DocumentState | null;
  viewport: ViewportState | null;
  disabled: boolean;
  isSetupPanelOpen: boolean;
  selectedOperationId: string | null;
  showStock: boolean;
  wcsOrientation: string;
  setShowStock: (show: boolean) => void;
  setWcsOrientation: (mode: string) => void;
  setSetupPanelOpen: (open: boolean) => void;
  setSelectedOperationId: (operationId: string | null) => void;
  runAction: RunAction;
  addMessage: (message: string) => void;
  onExportGcode: () => void;
  onPostProcessorChange: (postType: string) => void;
  postProcessorType: string;
  posts: Array<{ name: string; path: string }>;
  onImportPost: () => void;
  onEditPost: (path: string) => void;
  camSetupCreate: (setup: CamSetup) => Promise<void>;
  camSetupUpdate: (setup: CamSetup) => Promise<void>;
  camOperationUpdate: (
    opId: string,
    partial: Partial<CamOperation>,
  ) => Promise<void>;
  camOperationDelete: (opId: string) => Promise<void>;
  camOperationPreview: (opId: string) => Promise<void>;
  camOperationGenerate: (opId: string) => Promise<void>;
}

export function CamFloatingPanels({
  document,
  viewport,
  disabled,
  isSetupPanelOpen,
  selectedOperationId,
  showStock,
  wcsOrientation,
  setShowStock,
  setWcsOrientation,
  setSetupPanelOpen,
  setSelectedOperationId,
  runAction,
  addMessage,
  onExportGcode,
  onPostProcessorChange,
  postProcessorType,
  posts,
  onImportPost,
  onEditPost,
  camSetupCreate,
  camSetupUpdate,
  camOperationUpdate,
  camOperationDelete,
  camOperationPreview,
  camOperationGenerate,
}: CamFloatingPanelsProps) {
  const setup = document?.cam.setups?.[0] ?? null;

  const setupPanel = isSetupPanelOpen ? (
    <CamSetupPanel
      initialSetup={setup ?? createDefaultCamSetup()}
      bodies={(viewport?.bodies ?? []).map((body) => ({
        id: body.id,
        label: body.label,
        center: body.center,
        size: body.size,
      }))}
      showStock={showStock}
      onShowStockChange={setShowStock}
      wcsOrientation={wcsOrientation}
      onWcsOrientationChange={setWcsOrientation}
      postProcessorType={postProcessorType}
      onPostProcessorChange={onPostProcessorChange}
      posts={posts}
      onImportPost={onImportPost}
      onEditPost={onEditPost}
      disabled={disabled}
      onUpdate={(nextSetup) => {
        void runAction(async () => {
          if (setup) {
            await camSetupUpdate(nextSetup);
          } else {
            await camSetupCreate(nextSetup);
          }
        });
      }}
      onConfirm={() => setSetupPanelOpen(false)}
      onCancel={() => setSetupPanelOpen(false)}
    />
  ) : null;

  const operationPanel = selectedOperationId
    ? buildOperationPanel({
        document,
        disabled,
        selectedOperationId,
        setSelectedOperationId,
        runAction,
        addMessage,
        onExportGcode,
        camOperationUpdate,
        camOperationDelete,
        camOperationPreview,
        camOperationGenerate,
      })
    : null;

  return (
    <>
      {setupPanel}
      {operationPanel}
    </>
  );
}

function buildOperationPanel({
  document,
  disabled,
  selectedOperationId,
  setSelectedOperationId,
  runAction,
  addMessage,
  onExportGcode,
  camOperationUpdate,
  camOperationDelete,
  camOperationPreview,
  camOperationGenerate,
}: Pick<
  CamFloatingPanelsProps,
  | "document"
  | "disabled"
  | "selectedOperationId"
  | "setSelectedOperationId"
  | "runAction"
  | "addMessage"
  | "onExportGcode"
  | "camOperationUpdate"
  | "camOperationDelete"
  | "camOperationPreview"
  | "camOperationGenerate"
>) {
  const operation = document?.cam.operations.find(
    (candidate) => candidate.op_id === selectedOperationId,
  );
  if (!operation) {
    return null;
  }

  const tool = document?.cam.tool_library.find(
    (entry) => entry.tool_id === operation.tool_id,
  );
  const toolpathStats =
    operation.toolpath_cache?.total_length_mm !== undefined ||
    operation.toolpath_cache?.estimated_time_seconds !== undefined
      ? {
          totalLengthMm: operation.toolpath_cache?.total_length_mm,
          estimatedTimeSeconds: operation.toolpath_cache?.estimated_time_seconds,
        }
      : null;

  const shared = {
    operationName: operation.name,
    status: operation.status,
    statusMessage: operation.status_message,
    toolpathStats,
    disabled,
  };

  if (operation.type === "laser_cut") {
    const laser = operation.parameters.laser ?? DEFAULT_LASER_PARAMS;
    return (
      <CamLaserCutPanel
        {...shared}
        initialParams={laser}
        initialFeedrate={operation.parameters.feedrate_mm_per_min ?? 500}
        onUpdate={(partial: Partial<LaserCutParameters>) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              parameters: {
                ...operation.parameters,
                laser: { ...laser, ...partial },
              },
            });
          });
        }}
        onFeedrateChange={(feedrateMmPerMin: number) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              parameters: {
                ...operation.parameters,
                feedrate_mm_per_min: feedrateMmPerMin,
              },
            });
          });
        }}
        onPreview={() => {
          void runAction(async () => {
            await camOperationPreview(operation.op_id);
          });
        }}
        onGenerate={() => {
          void runAction(async () => {
            await camOperationGenerate(operation.op_id);
            // Wait for the generated document and confirm with the
            // path stats — the toolpath looks identical to the
            // preview, so the feedback has to be explicit.
            try {
              const updated = await awaitDocumentChange(
                (next) =>
                  next.cam.operations.find(
                    (candidate) => candidate.op_id === operation.op_id,
                  )?.status === "generated",
              );
              const generated = updated.cam.operations.find(
                (candidate) => candidate.op_id === operation.op_id,
              );
              const stats = generated?.toolpath_cache;
              if (stats) {
                const length =
                  stats.total_length_mm !== undefined
                    ? `${stats.total_length_mm.toFixed(1)} mm`
                    : "?";
                const time =
                  stats.estimated_time_seconds !== undefined
                    ? `${stats.estimated_time_seconds.toFixed(1)} s`
                    : "?";
                addMessage(
                  `toolpath generated: ${length}, estimated ${time}`,
                );
              }
            } catch {
              // The operation degraded instead of generating — the
              // panel's status line shows why.
            }
          });
        }}
        onExport={onExportGcode}
        onDelete={() => {
          void runAction(async () => {
            await camOperationDelete(operation.op_id);
            setSelectedOperationId(null);
          });
        }}
        onClose={() => setSelectedOperationId(null)}
      />
    );
  }

  if (operation.type === "face_milling") {
    const parameters = operation.parameters;
    const initialParams: FaceMillingFormState = {
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ?? DEFAULT_FACE_MILLING_PARAMS.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_FACE_MILLING_PARAMS.plunge_feedrate_mm_per_min,
      stepover_percent:
        parameters.stepover_percent ?? DEFAULT_FACE_MILLING_PARAMS.stepover_percent,
      zigzag_angle_deg:
        parameters.zigzag_angle_deg ?? DEFAULT_FACE_MILLING_PARAMS.zigzag_angle_deg,
      spindle_rpm: parameters.spindle_rpm ?? DEFAULT_FACE_MILLING_PARAMS.spindle_rpm,
    };
    const tools = document?.cam.tool_library.filter(
      (entry) => entry.type === "endmill_flat",
    ) ?? [];
    return (
      <CamFaceMillingPanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        onUpdate={(partial, toolId) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              tool_id: toolId,
              parameters: { ...parameters, ...partial },
            });
          });
        }}
        onPreview={() => {
          void runAction(async () => {
            await camOperationPreview(operation.op_id);
          });
        }}
        onGenerate={() => {
          void runAction(async () => {
            await camOperationGenerate(operation.op_id);
            // Wait for the generated document and confirm with the
            // path stats — the toolpath looks identical to the
            // preview, so the feedback has to be explicit.
            try {
              const updated = await awaitDocumentChange(
                (next) =>
                  next.cam.operations.find(
                    (candidate) => candidate.op_id === operation.op_id,
                  )?.status === "generated",
              );
              const generated = updated.cam.operations.find(
                (candidate) => candidate.op_id === operation.op_id,
              );
              const stats = generated?.toolpath_cache;
              if (stats) {
                const length =
                  stats.total_length_mm !== undefined
                    ? `${stats.total_length_mm.toFixed(1)} mm`
                    : "?";
                const time =
                  stats.estimated_time_seconds !== undefined
                    ? `${stats.estimated_time_seconds.toFixed(1)} s`
                    : "?";
                addMessage(
                  `toolpath generated: ${length}, estimated ${time}`,
                );
              }
            } catch {
              // The operation degraded instead of generating — the
              // panel's status line shows why.
            }
          });
        }}
        onExport={onExportGcode}
        onDelete={() => {
          void runAction(async () => {
            await camOperationDelete(operation.op_id);
            setSelectedOperationId(null);
          });
        }}
        onClose={() => setSelectedOperationId(null)}
      />
    );
  }

  // Unsupported operation kinds get no panel yet — the sidebar list
  // still shows them, and generation is not offered for them.
  return null;
}
