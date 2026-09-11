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
  CamAdaptivePanel,
  CamContourPanel,
  CamDrillingPanel,
  CamEngravePanel,
  CamFaceMillingPanel,
  CamLaserCutPanel,
  CamPocketPanel,
  CamSetupPanel,
  CamSlotPanel,
  CamGrblPanel,
  CamTestPatternPanel,
  createDefaultCamSetup,
  type AdaptiveFormState,
  type ContourFormState,
  type DrillPointRow,
  type DrillingFormState,
  type EngraveFormState,
  type FaceMillingFormState,
  type PocketFormState,
  type SlotFormState,
} from "../layout";
import { DEFAULT_ADAPTIVE_PARAMS } from "../layout/CamAdaptivePanel";
import { DEFAULT_FACE_MILLING_PARAMS } from "../layout/CamFaceMillingPanel";
import { DEFAULT_POCKET_PARAMS } from "../layout/CamPocketPanel";
import {
  DEFAULT_CONTOUR_FORM,
  DEFAULT_CONTOUR_PARAMS,
} from "../layout/CamContourPanel";
import { DEFAULT_DRILLING_PARAMS } from "../layout/CamDrillingPanel";
import { DEFAULT_SLOT_PARAMS } from "../layout/CamSlotPanel";
import {
  DEFAULT_ENGRAVE_FORM,
  DEFAULT_ENGRAVE_PARAMS,
} from "../layout/CamEngravePanel";
import { DEFAULT_LASER_PARAMS } from "../layout/CamLaserCutPanel";
import { DEFAULT_TEST_PATTERN_PARAMS } from "../layout/CamTestPatternPanel";
import { awaitDocumentChange } from "../state/cadCoreStore";
import {
  contourOperationInputKind,
  laserOperationScopeSketchId,
} from "./camProfileSelection";

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
  // Shell-side GRBL streaming panel (serial transport to the machine).
  isGrblPanelOpen: boolean;
  setGrblPanelOpen: (open: boolean) => void;
  onOpenGrblControls: () => void;
  setSelectedOperationId: (operationId: string | null) => void;
  runAction: RunAction;
  addMessage: (message: string) => void;
  onExportGcode: () => void;
  // Export + external LaserGRBL launch (laser panels only).
  onExportAndOpen: () => void;
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
  // Armed pocket face pick — opId + which face it will replace.
  // Adaptive Clearing reuses the pick; `kind` picks the toast copy.
  pocketPick: {
    opId: string;
    target: "outer" | "island";
    kind: "pocket" | "adaptive";
  } | null;
  onPickPocketFace: (opId: string, kind?: "pocket" | "adaptive") => void;
  onPickIslandFace: (opId: string, kind?: "pocket" | "adaptive") => void;
  onCancelPocketPick: () => void;
  // Armed contour face pick — opId whose machining region the next
  // body-face click replaces.
  contourPick: { opId: string } | null;
  onPickContourFace: (opId: string) => void;
  onCancelContourPick: () => void;
  // Armed drilling point pick — opId whose hole locations gain the
  // next viewport point click.  Stays armed across adds.
  drillPick: { opId: string } | null;
  onPickDrillPoint: (opId: string) => void;
  onCancelDrillPick: () => void;
  // Slot Re-pick — re-captures the CURRENT edge selection (no armed
  // pick state; selection is already the input).
  onRepickSlotEdges: (opId: string) => void;
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
  isGrblPanelOpen,
  setGrblPanelOpen,
  onOpenGrblControls,
  setSelectedOperationId,
  runAction,
  addMessage,
  onExportGcode,
  onExportAndOpen,
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
  pocketPick,
  onPickPocketFace,
  onPickIslandFace,
  onCancelPocketPick,
  contourPick,
  onPickContourFace,
  onCancelContourPick,
  drillPick,
  onPickDrillPoint,
  onCancelDrillPick,
  onRepickSlotEdges,
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
      onOpenGrblControls={onOpenGrblControls}
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
        onExportAndOpen,
        camOperationUpdate,
        camOperationDelete,
        camOperationSetScope,
        camOperationPreview,
        camOperationGenerate,
        pocketPick,
        onPickPocketFace,
        onPickIslandFace,
        onCancelPocketPick,
        contourPick,
        onPickContourFace,
        onCancelContourPick,
        drillPick,
        onPickDrillPoint,
        onCancelDrillPick,
        onRepickSlotEdges,
        t,
      })
    : null;

  // Shell-side GRBL streaming — independent of document CAM state, so
  // it gets its own floating panel branch.
  const grblPanel = isGrblPanelOpen ? (
    <CamGrblPanel onClose={() => setGrblPanelOpen(false)} />
  ) : null;

  return (
    <>
      {setupPanel}
      {operationPanel}
      {grblPanel}
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
  onExportAndOpen,
  camOperationUpdate,
  camOperationDelete,
  camOperationSetScope,
  camOperationPreview,
  camOperationGenerate,
  pocketPick,
  onPickPocketFace,
  onPickIslandFace,
  onCancelPocketPick,
  contourPick,
  onPickContourFace,
  onCancelContourPick,
  drillPick,
  onPickDrillPoint,
  onCancelDrillPick,
  onRepickSlotEdges,
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
  | "onExportAndOpen"
  | "camOperationUpdate"
  | "camOperationDelete"
  | "camOperationSetScope"
  | "camOperationPreview"
  | "camOperationGenerate"
  | "pocketPick"
  | "onPickPocketFace"
  | "onPickIslandFace"
  | "onCancelPocketPick"
  | "contourPick"
  | "onPickContourFace"
  | "onCancelContourPick"
  | "drillPick"
  | "onPickDrillPoint"
  | "onCancelDrillPick"
  | "onRepickSlotEdges"
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
        onExportAndOpen={onExportAndOpen}
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
        onExportAndOpen={onExportAndOpen}
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
      // Absent in the params = single pass; never substitute a default
      // number for a cleared stepdown.
      stepdown_mm: parameters.stepdown_mm,
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

  if (operation.type === "pocket_2d") {
    const parameters = operation.parameters;
    const initialParams: PocketFormState = {
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ?? DEFAULT_POCKET_PARAMS.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_POCKET_PARAMS.plunge_feedrate_mm_per_min,
      stepover_percent:
        parameters.stepover_percent ?? DEFAULT_POCKET_PARAMS.stepover_percent,
      zigzag_angle_deg:
        parameters.zigzag_angle_deg ?? DEFAULT_POCKET_PARAMS.zigzag_angle_deg,
      spindle_rpm: parameters.spindle_rpm ?? DEFAULT_POCKET_PARAMS.spindle_rpm,
      // Absent in the params = single pass; never substitute a default
      // number for a cleared stepdown.
      stepdown_mm: parameters.stepdown_mm,
    };
    const tools =
      document?.cam.tool_library.filter(
        (entry) => entry.type === "endmill_flat",
      ) ?? [];
    const avoidance = operation.geometry_references.avoidance_regions;
    return (
      <CamPocketPanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        islandCount={avoidance.length}
        pickTarget={
          pocketPick?.opId === operation.op_id ? pocketPick.target : null
        }
        onRepickFace={() => onPickPocketFace(operation.op_id)}
        onAddIsland={() => onPickIslandFace(operation.op_id)}
        onRemoveIsland={(index) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              geometry_references: {
                machining_regions:
                  operation.geometry_references.machining_regions,
                avoidance_regions: avoidance.filter(
                  (_, entryIndex) => entryIndex !== index,
                ),
                guide_curves: [],
                check_surfaces: [],
              },
            });
          });
        }}
        onCancelPick={onCancelPocketPick}
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

  if (operation.type === "adaptive_clearing") {
    const parameters = operation.parameters;
    const initialParams: AdaptiveFormState = {
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ??
        DEFAULT_ADAPTIVE_PARAMS.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_ADAPTIVE_PARAMS.plunge_feedrate_mm_per_min,
      stepover_percent:
        parameters.stepover_percent ??
        DEFAULT_ADAPTIVE_PARAMS.stepover_percent,
      spindle_rpm:
        parameters.spindle_rpm ?? DEFAULT_ADAPTIVE_PARAMS.spindle_rpm,
      // Absent in the params = single pass; never substitute a default
      // number for a cleared stepdown.
      stepdown_mm: parameters.stepdown_mm,
    };
    const tools =
      document?.cam.tool_library.filter(
        (entry) => entry.type === "endmill_flat",
      ) ?? [];
    const avoidance = operation.geometry_references.avoidance_regions;
    return (
      <CamAdaptivePanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        islandCount={avoidance.length}
        pickTarget={
          pocketPick?.opId === operation.op_id ? pocketPick.target : null
        }
        onRepickFace={() => onPickPocketFace(operation.op_id, "adaptive")}
        onAddIsland={() => onPickIslandFace(operation.op_id, "adaptive")}
        onRemoveIsland={(index) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              geometry_references: {
                machining_regions:
                  operation.geometry_references.machining_regions,
                avoidance_regions: avoidance.filter(
                  (_, entryIndex) => entryIndex !== index,
                ),
                guide_curves: [],
                check_surfaces: [],
              },
            });
          });
        }}
        onCancelPick={onCancelPocketPick}
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

  if (operation.type === "contour_2d") {
    const parameters = operation.parameters;
    const contour = parameters.contour ?? DEFAULT_CONTOUR_PARAMS;
    const initialParams: ContourFormState = {
      side: contour.side ?? DEFAULT_CONTOUR_FORM.side,
      depth_mm: contour.depth_mm ?? DEFAULT_CONTOUR_FORM.depth_mm,
      stock_allowance_mm:
        contour.stock_allowance_mm ?? DEFAULT_CONTOUR_FORM.stock_allowance_mm,
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ?? DEFAULT_CONTOUR_FORM.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_CONTOUR_FORM.plunge_feedrate_mm_per_min,
      spindle_rpm: parameters.spindle_rpm ?? DEFAULT_CONTOUR_FORM.spindle_rpm,
    };
    const tools =
      document?.cam.tool_library.filter(
        (entry) => entry.type === "endmill_flat",
      ) ?? [];
    // Same scope logic as laser: every profile-attesting machining
    // region must come from one sketch for the dropdown to have a scope.
    const scopeSketchId = laserOperationScopeSketchId(operation);
    const inputKind = contourOperationInputKind(operation);
    const sketches = (document?.feature_history ?? [])
      .filter((feature) => feature.kind === "sketch")
      .map((feature) => ({
        feature_id: feature.feature_id,
        name: feature.name || "Sketch",
      }));
    return (
      <CamContourPanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        inputKind={inputKind}
        geometryCount={operation.geometry_references.machining_regions.length}
        selectedProfileCount={
          document?.selected_sketch_profile_ids?.length ?? 0
        }
        repickArmed={camProfilePickArmed}
        sketches={sketches}
        scopeSketchId={scopeSketchId}
        onSetScope={(featureId) => {
          void runAction(async () => {
            await camOperationSetScope(operation.op_id, featureId);
          });
        }}
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
        facePickArmed={
          contourPick?.opId === operation.op_id && inputKind === "face"
        }
        onRepickFace={() => onPickContourFace(operation.op_id)}
        onCancelFacePick={onCancelContourPick}
        onUpdate={(partial, toolId) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              tool_id: toolId,
              parameters: {
                ...parameters,
                feedrate_mm_per_min:
                  partial.feedrate_mm_per_min ?? parameters.feedrate_mm_per_min,
                plunge_feedrate_mm_per_min:
                  partial.plunge_feedrate_mm_per_min ??
                  parameters.plunge_feedrate_mm_per_min,
                spindle_rpm: partial.spindle_rpm ?? parameters.spindle_rpm,
                contour: {
                  ...contour,
                  side: partial.side ?? contour.side,
                  depth_mm: partial.depth_mm ?? contour.depth_mm,
                  stock_allowance_mm:
                    partial.stock_allowance_mm ?? contour.stock_allowance_mm,
                },
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

  if (operation.type === "drilling") {
    const parameters = operation.parameters;
    const initialParams: DrillingFormState = {
      cycle_type:
        parameters.cycle_type ?? DEFAULT_DRILLING_PARAMS.cycle_type,
      hole_depth_mm:
        parameters.hole_depth_mm ?? DEFAULT_DRILLING_PARAMS.hole_depth_mm,
      peck_depth_mm:
        parameters.peck_depth_mm ?? DEFAULT_DRILLING_PARAMS.peck_depth_mm,
      through_hole: parameters.through_hole ?? false,
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ??
        DEFAULT_DRILLING_PARAMS.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_DRILLING_PARAMS.plunge_feedrate_mm_per_min,
      spindle_rpm:
        parameters.spindle_rpm ?? DEFAULT_DRILLING_PARAMS.spindle_rpm,
    };
    const tools =
      document?.cam.tool_library.filter((entry) => entry.type === "drill") ??
      [];
    const regions = operation.geometry_references.machining_regions;
    // User-facing row labels only — a captured wall/rim names itself by
    // kind, free picks show their coordinates, and legacy sketch-circle
    // refs (no longer drilling inputs) tell the user to re-pick on the
    // body.  Internal ids never reach the panel.
    const points: DrillPointRow[] = regions.map((region, index) => {
      const attestation = region.attestation;
      if (
        attestation &&
        "point" in attestation &&
        Array.isArray(attestation.point)
      ) {
        const [x, y, z] = attestation.point;
        return {
          kind: "point",
          label: `X ${x.toFixed(2)}  Y ${y.toFixed(2)}  Z ${z.toFixed(2)}`,
        };
      }
      if (attestation && "sample_points" in attestation) {
        return {
          kind: "wall",
          label: t("cam.drilling.wallRow", { index: index + 1 }),
        };
      }
      if (attestation && "start_point" in attestation) {
        return {
          kind: "rim",
          label: t("cam.drilling.rimRow", { index: index + 1 }),
        };
      }
      return {
        kind: "legacy",
        label: t("cam.drilling.legacyRow", { index: index + 1 }),
      };
    });
    return (
      <CamDrillingPanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        points={points}
        pickArmed={drillPick?.opId === operation.op_id}
        onPickPoint={() => onPickDrillPoint(operation.op_id)}
        onCancelPick={onCancelDrillPick}
        onRemovePoint={(index) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              geometry_references: {
                ...operation.geometry_references,
                machining_regions: regions.filter(
                  (_, entryIndex) => entryIndex !== index,
                ),
              },
            });
          });
        }}
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

  if (operation.type === "slot") {
    const parameters = operation.parameters;
    const slot = parameters.slot ?? { depth_mm: DEFAULT_SLOT_PARAMS.depth_mm };
    const initialParams: SlotFormState = {
      depth_mm: slot.depth_mm ?? DEFAULT_SLOT_PARAMS.depth_mm,
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ?? DEFAULT_SLOT_PARAMS.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_SLOT_PARAMS.plunge_feedrate_mm_per_min,
      spindle_rpm:
        parameters.spindle_rpm ?? DEFAULT_SLOT_PARAMS.spindle_rpm,
      // Absent in the params = single pass; never substitute a default
      // number for a cleared stepdown.
      stepdown_mm: parameters.stepdown_mm,
    };
    const tools =
      document?.cam.tool_library.filter(
        (entry) => entry.type === "endmill_flat",
      ) ?? [];
    const regions = operation.geometry_references.machining_regions;
    return (
      <CamSlotPanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        edgeCount={regions.length}
        onRepickEdges={() => onRepickSlotEdges(operation.op_id)}
        onRemoveEdge={(index) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              geometry_references: {
                ...operation.geometry_references,
                machining_regions: regions.filter(
                  (_, entryIndex) => entryIndex !== index,
                ),
              },
            });
          });
        }}
        onUpdate={(partial, toolId) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              tool_id: toolId,
              parameters: {
                ...parameters,
                feedrate_mm_per_min:
                  partial.feedrate_mm_per_min ??
                  parameters.feedrate_mm_per_min,
                plunge_feedrate_mm_per_min:
                  partial.plunge_feedrate_mm_per_min ??
                  parameters.plunge_feedrate_mm_per_min,
                spindle_rpm:
                  partial.spindle_rpm ?? parameters.spindle_rpm,
                stepdown_mm:
                  partial.stepdown_mm !== undefined
                    ? partial.stepdown_mm
                    : parameters.stepdown_mm,
                slot: {
                  ...slot,
                  depth_mm: partial.depth_mm ?? slot.depth_mm,
                },
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

  if (operation.type === "engrave") {
    const parameters = operation.parameters;
    const engrave = parameters.engrave ?? DEFAULT_ENGRAVE_PARAMS;
    const initialParams: EngraveFormState = {
      depth_mm: engrave.depth_mm ?? DEFAULT_ENGRAVE_FORM.depth_mm,
      feedrate_mm_per_min:
        parameters.feedrate_mm_per_min ??
        DEFAULT_ENGRAVE_FORM.feedrate_mm_per_min,
      plunge_feedrate_mm_per_min:
        parameters.plunge_feedrate_mm_per_min ??
        DEFAULT_ENGRAVE_FORM.plunge_feedrate_mm_per_min,
      spindle_rpm:
        parameters.spindle_rpm ?? DEFAULT_ENGRAVE_FORM.spindle_rpm,
    };
    const tools =
      document?.cam.tool_library.filter(
        (entry) => entry.type === "endmill_flat",
      ) ?? [];
    // Sketch-profile geometry only — the same scope logic as the
    // contour profile branch.
    const scopeSketchId = laserOperationScopeSketchId(operation);
    const sketches = (document?.feature_history ?? [])
      .filter((feature) => feature.kind === "sketch")
      .map((feature) => ({
        feature_id: feature.feature_id,
        name: feature.name || "Sketch",
      }));
    return (
      <CamEngravePanel
        {...shared}
        initialParams={initialParams}
        initialToolId={operation.tool_id}
        tools={tools}
        geometryCount={operation.geometry_references.machining_regions.length}
        selectedProfileCount={
          document?.selected_sketch_profile_ids?.length ?? 0
        }
        repickArmed={camProfilePickArmed}
        sketches={sketches}
        scopeSketchId={scopeSketchId}
        onSetScope={(featureId) => {
          void runAction(async () => {
            await camOperationSetScope(operation.op_id, featureId);
          });
        }}
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
        onUpdate={(partial, toolId) => {
          void runAction(async () => {
            await camOperationUpdate(operation.op_id, {
              tool_id: toolId,
              parameters: {
                ...parameters,
                feedrate_mm_per_min:
                  partial.feedrate_mm_per_min ??
                  parameters.feedrate_mm_per_min,
                plunge_feedrate_mm_per_min:
                  partial.plunge_feedrate_mm_per_min ??
                  parameters.plunge_feedrate_mm_per_min,
                spindle_rpm:
                  partial.spindle_rpm ?? parameters.spindle_rpm,
                engrave: {
                  ...engrave,
                  depth_mm: partial.depth_mm ?? engrave.depth_mm,
                },
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

  // Unsupported operation kinds get no panel yet — the sidebar list
  // still shows them, and generation is not offered for them.
  return null;
}
