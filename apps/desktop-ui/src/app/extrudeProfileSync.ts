import type { ActiveExtrudeAction } from "./appState";
import type { DefaultExtrudeSettings } from "./extrudeDefaults";
import type { ExtrudeMode } from "../types";
import type { DocumentState } from "../types/ipc";

interface MutableRef<T> {
  current: T;
}

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

export interface ExtrudeProfileSelectionSyncContext {
  extrudeAction: ActiveExtrudeAction | null;
  document: DocumentState | null;
  selectedSketchProfileIds: readonly string[];
  selectedSketchProfileIdsKey: string;
  selectedExtrudableFaceId: string | null;
  lastExtrudeProfileUpdateRef: MutableRef<string>;
  getDefaultExtrudeSettings: (
    profileIds: readonly string[],
    faceIdOverride?: string | null,
  ) => DefaultExtrudeSettings;
  createExtrudeFromSelectedFace: (
    faceId: string,
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
  ) => Promise<void>;
  createExtrudeFromSelectedProfiles: (
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
  ) => Promise<void>;
  updateExtrudeProfiles: (
    featureId: string,
    profileIds: readonly string[],
  ) => Promise<void>;
  setExtrudeAction: (
    updater: (
      current: ActiveExtrudeAction | null,
    ) => ActiveExtrudeAction | null,
  ) => void;
  runAction: RunAction;
}

export function syncExtrudeProfileSelection({
  extrudeAction,
  ...context
}: ExtrudeProfileSelectionSyncContext) {
  if (!extrudeAction) {
    return;
  }
  if (context.selectedSketchProfileIds.length === 0) {
    syncPendingFaceExtrude(extrudeAction, context);
    return;
  }
  if (extrudeAction.phase === "pending") {
    syncPendingProfileExtrude(extrudeAction, context);
    return;
  }
  syncActiveProfileExtrude(extrudeAction, context);
}

function syncPendingFaceExtrude(
  extrudeAction: ActiveExtrudeAction,
  context: Omit<ExtrudeProfileSelectionSyncContext, "extrudeAction">,
) {
  if (extrudeAction.phase !== "pending" || !context.selectedExtrudableFaceId) {
    return;
  }
  const defaultSettings = context.getDefaultExtrudeSettings(
    [],
    context.selectedExtrudableFaceId,
  );
  const { mode, targetBodyId } = resolveInitialExtrudeTarget(
    extrudeAction,
    defaultSettings,
  );
  void context.createExtrudeFromSelectedFace(
    context.selectedExtrudableFaceId,
    extrudeAction.initialDepth,
    extrudeAction.automaticMode ? null : mode,
    extrudeAction.automaticMode ? null : targetBodyId,
  );
}

function syncPendingProfileExtrude(
  extrudeAction: ActiveExtrudeAction,
  context: Omit<ExtrudeProfileSelectionSyncContext, "extrudeAction">,
) {
  const defaultSettings = context.getDefaultExtrudeSettings(
    context.selectedSketchProfileIds,
  );
  const { mode, targetBodyId } = resolveInitialExtrudeTarget(
    extrudeAction,
    defaultSettings,
  );
  void context.createExtrudeFromSelectedProfiles(
    extrudeAction.initialDepth,
    extrudeAction.automaticMode ? null : mode,
    extrudeAction.automaticMode ? null : targetBodyId,
  );
}

function syncActiveProfileExtrude(
  extrudeAction: ActiveExtrudeAction,
  context: Omit<ExtrudeProfileSelectionSyncContext, "extrudeAction">,
) {
  if (!extrudeAction.featureId) {
    return;
  }
  // During cancel, an undo may remove the feature before we process
  // the pending profile-sync update.  When the feature is already gone
  // there is nothing to update — bail out silently.
  if (
    context.document &&
    !context.document.feature_history.some(
      (f) => f.feature_id === extrudeAction.featureId,
    )
  ) {
    return;
  }
  if (
    context.lastExtrudeProfileUpdateRef.current ===
    context.selectedSketchProfileIdsKey
  ) {
    return;
  }
  context.lastExtrudeProfileUpdateRef.current =
    context.selectedSketchProfileIdsKey;
  const nextCount = context.selectedSketchProfileIds.length;
  void context.runAction(async () => {
    await context.updateExtrudeProfiles(
      extrudeAction.featureId!,
      context.selectedSketchProfileIds,
    );
    context.setExtrudeAction((current) =>
      current?.phase === "active" && current.featureId === extrudeAction.featureId
        ? {
            ...current,
            profileIds: [...context.selectedSketchProfileIds],
            profileCount: nextCount,
          }
        : current,
    );
  });
}

function resolveInitialExtrudeTarget(
  extrudeAction: ActiveExtrudeAction,
  defaultSettings: DefaultExtrudeSettings,
) {
  const mode =
    extrudeAction.initialMode === "new_body"
      ? defaultSettings.mode
      : extrudeAction.initialMode;
  const targetBodyId =
    mode === "new_body"
      ? null
      : extrudeAction.initialTargetBodyId ?? defaultSettings.targetBodyId;
  return { mode, targetBodyId };
}
