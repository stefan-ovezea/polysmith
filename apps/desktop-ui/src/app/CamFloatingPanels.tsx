import { useTranslation } from "react-i18next";

import type {
  CamOperation,
  CamSetup,
  LaserCutParameters,
  LaserMachineSettings,
  MachineDefinition,
  MachineType,
  PostProcessor,
  PostProcessorType,
} from "@/types";
import type { DocumentState, ViewportState } from "../types";
import {
  CamFaceMillingPanel,
  CamLaserCutPanel,
  CamSetupPanel,
  CamTestPatternPanel,
  createDefaultCamSetup,
  type FaceMillingFormState,
} from "../layout";
import { DEFAULT_FACE_MILLING_PARAMS } from "../layout/CamFaceMillingPanel";
import { DEFAULT_LASER_PARAMS } from "../layout/CamLaserCutPanel";
import { DEFAULT_TEST_PATTERN_PARAMS } from "../layout/CamTestPatternPanel";
import { awaitDocumentChange } from "../state/cadCoreStore";
import { laserOperationScopeSketchId } from "./camProfileSelection";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface CamFloatingPanelsProps {
  document: DocumentState | null;
  viewport: ViewportState | null;
  disabled: boolean;
  isSetupPanelOpen: boolean;
  selectedOperationId: string | null;
  activeSetupId: string | null;
  camProfilePickArmed: boolean;
  onStartRepickGeometry: () => void;
  onCancelRepickGeometry: () => void;
  onApplyRepickGeometry: () => void;
  // "Clear selection" button while re-pick is armed.
  onClearRepickSelection: () => void;
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
  machines: MachineDefinition[];
  onSaveMachine: (machine: MachineDefinition) => Promise<void>;
  camPostProcessorSet: (post: PostProcessor) => Promise<void>;
  onPickOrigin: () => void;
  pickedOrigin: [number, number, number] | null;
  originPickArmed: boolean;
  wcsPickArmed: boolean;
  onPickWcsFace: () => void;
  camSetupCreate: (setup: CamSetup) => Promise<void>;
  camSetupUpdate: (setup: CamSetup) => Promise<void>;
  camMachineSettingsSet: (settings: LaserMachineSettings) => Promise<void>;
  camOperationUpdate: (
    opId: string,
    partial: Partial<CamOperation>,
  ) => Promise<void>;
  camOperationDelete: (opId: string) => Promise<void>;
  camOperationSetScope: (opId: string, featureId: string) => Promise<void>;
  camOperationPreview: (opId: string) => Promise<void>;
  camOperationGenerate: (opId: string) => Promise<void>;
}

export function CamFloatingPanels({
  document,
  viewport,
  disabled,
  isSetupPanelOpen,
  selectedOperationId,
  activeSetupId,
  camProfilePickArmed,
  onStartRepickGeometry,
  onCancelRepickGeometry,
  onApplyRepickGeometry,
  onClearRepickSelection,
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
  machines,
  onSaveMachine,
  camPostProcessorSet,
  onPickOrigin,
  pickedOrigin,
  originPickArmed,
  wcsPickArmed,
  onPickWcsFace,
  camSetupCreate,
  camSetupUpdate,
  camMachineSettingsSet,
  camOperationUpdate,
  camOperationDelete,
  camOperationSetScope,
  camOperationPreview,
  camOperationGenerate,
}: CamFloatingPanelsProps) {
  const { t } = useTranslation();
  // Multi-setup: the setup panel edits the setup the SELECTED
  // operation belongs to; without a selection it edits the first.
  const setup = (() => {
    const setups = document?.cam.setups ?? [];
    if (activeSetupId) {
      const active = setups.find((s) => s.setup_id === activeSetupId);
      if (active) {
        return active;
      }
    }
    if (selectedOperationId) {
      const op = document?.cam.operations.find(
        (candidate) => candidate.op_id === selectedOperationId,
      );
      if (op?.setup_id) {
        const owned = setups.find((s) => s.setup_id === op.setup_id);
        if (owned) {
          return owned;
        }
      }
    }
    return setups[0] ?? null;
  })();

  // Applying a saved machine composes the existing commands in one
  // runAction: setup machine type (active setup), document post
  // processor (document-level in v1), and laser machine settings when
  // the machine is a laser.  The panel mirrors the type into its form
  // before calling this.
  const applyMachine = (machine: MachineDefinition) => {
    const target = setup ?? createDefaultCamSetup();
    void runAction(async () => {
      await camSetupUpdate({
        ...target,
        machine_type: machine.machine_type as MachineType,
      });
      await camPostProcessorSet({
        type: machine.post_processor.type as PostProcessorType,
        filename: machine.post_processor.filename ?? "",
      });
      if (machine.machine_type === "laser") {
        await camMachineSettingsSet({
          work_area_x_mm: machine.work_area_x_mm,
          work_area_y_mm: machine.work_area_y_mm,
          pointer_offset_x_mm: machine.pointer_offset_x_mm,
          pointer_offset_y_mm: machine.pointer_offset_y_mm,
        });
      }
      addMessage(t("cam.setup.machineApplied", { name: machine.name }));
    });
  };

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
      machines={machines}
      onApplyMachine={applyMachine}
      onSaveMachine={onSaveMachine}
      onPickOrigin={onPickOrigin}
      pickedOrigin={pickedOrigin}
      originPickArmed={originPickArmed}
      wcsPickArmed={wcsPickArmed}
      onPickWcsFace={onPickWcsFace}
      machineSettings={document?.cam.machine_settings ?? null}
      onMachineSettingsChange={(settings) => {
        void runAction(async () => {
          await camMachineSettingsSet(settings);
        });
      }}
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
        activeSetupId,
        camProfilePickArmed,
        onStartRepickGeometry,
        onCancelRepickGeometry,
        onApplyRepickGeometry,
        onClearRepickSelection,
        setSelectedOperationId,
        runAction,
        addMessage,
        onExportGcode,
        camOperationUpdate,
        camOperationDelete,
        camOperationSetScope,
        camOperationPreview,
        camOperationGenerate,
        t,
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
  activeSetupId,
  camProfilePickArmed,
  onStartRepickGeometry,
  onCancelRepickGeometry,
  onApplyRepickGeometry,
  onClearRepickSelection,
  setSelectedOperationId,
  runAction,
  addMessage,
  onExportGcode,
  camOperationUpdate,
  camOperationDelete,
  camOperationSetScope,
  camOperationPreview,
  camOperationGenerate,
  t,
}: Pick<
  CamFloatingPanelsProps,
  | "document"
  | "disabled"
  | "selectedOperationId"
  | "activeSetupId"
  | "camProfilePickArmed"
  | "onStartRepickGeometry"
  | "onCancelRepickGeometry"
  | "onApplyRepickGeometry"
  | "onClearRepickSelection"
  | "setSelectedOperationId"
  | "runAction"
  | "addMessage"
  | "onExportGcode"
  | "camOperationUpdate"
  | "camOperationDelete"
  | "camOperationSetScope"
  | "camOperationPreview"
  | "camOperationGenerate"
> & { t: (key: string, options?: Record<string, unknown>) => string }) {
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

  // Shared generate handler (laser + face milling): run the generator,
  // wait for the refreshed document, and confirm through the message
  // log with the path stats — the toolpath looks identical to the
  // preview, so the feedback has to be explicit.
  const makeGenerateHandler = (opId: string) => () => {
    void runAction(async () => {
      await camOperationGenerate(opId);
      try {
        const updated = await awaitDocumentChange(
          (next) =>
            next.cam.operations.find((candidate) => candidate.op_id === opId)
              ?.status === "generated",
        );
        const generated = updated.cam.operations.find(
          (candidate) => candidate.op_id === opId,
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
          addMessage(t("cam.toolpathGenerated", { length, time }));
        }
      } catch {
        // The operation degraded instead of generating — the panel's
        // status line shows why.
      }
    });
  };

  if (operation.type === "laser_cut") {
    const laser = operation.parameters.laser ?? DEFAULT_LASER_PARAMS;
    // The reference sketch shown in the scope dropdown: every machining
    // region must attest the same sketch (mixed, face-based, or empty →
    // no scope).
    const scopeSketchId = laserOperationScopeSketchId(operation);
    const sketches = (document?.feature_history ?? [])
      .filter((feature) => feature.kind === "sketch")
      .map((feature) => ({
        feature_id: feature.feature_id,
        name: feature.name || "Sketch",
      }));
    return (
      <CamLaserCutPanel
        {...shared}
        initialParams={laser}
        initialFeedrate={operation.parameters.feedrate_mm_per_min ?? 500}
        geometryCount={operation.geometry_references.machining_regions.length}
        selectedProfileCount={
          document?.selected_sketch_profile_ids?.length ?? 0
        }
        repickArmed={camProfilePickArmed}
        onStartRepick={onStartRepickGeometry}
        onCancelRepick={onCancelRepickGeometry}
        onClearSelection={onClearRepickSelection}
        onApplyRepick={() => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              geometry_references: {
                machining_regions: [],
                avoidance_regions: [],
                guide_curves: [],
                check_surfaces: [],
              },
            });
            onApplyRepickGeometry();
          });
        }}
        sketches={sketches}
        scopeSketchId={scopeSketchId}
        onSetScope={(featureId) => {
          void runAction(async () => {
            await camOperationSetScope(operation.op_id, featureId);
          });
        }}
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
        onPreview={() => {
          void runAction(async () => {
            await camOperationPreview(operation.op_id);
          });
        }}
        onGenerate={makeGenerateHandler(operation.op_id)}
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

  if (operation.type === "laser_test_pattern") {
    const pattern =
      operation.parameters.test_pattern ?? DEFAULT_TEST_PATTERN_PARAMS;
    return (
      <CamTestPatternPanel
        {...shared}
        initialParams={pattern}
        onUpdate={(partial) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              parameters: {
                ...operation.parameters,
                test_pattern: { ...pattern, ...partial },
              },
            });
          });
        }}
        onPreview={() => {
          void runAction(async () => {
            await camOperationPreview(operation.op_id);
          });
        }}
        onGenerate={makeGenerateHandler(operation.op_id)}
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
        onGenerate={makeGenerateHandler(operation.op_id)}
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
