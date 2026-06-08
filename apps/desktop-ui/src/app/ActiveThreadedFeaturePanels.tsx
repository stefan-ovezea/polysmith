import type {
  FastenerFeatureParameters,
  HelixFeatureParameters,
  ThreadFeatureParameters,
} from "../types";
import type { HoleStandardEntry } from "../lib";
import type {
  FastenerAction,
  HelixAction,
  ThreadAction,
} from "./appState";
import { ActiveFastenerPanel } from "./ActiveFastenerPanel";
import { ActiveHelixPanel } from "./ActiveHelixPanel";
import { ActiveThreadPanel } from "./ActiveThreadPanel";
import * as selectionSources from "./selectionSources";

type AxisSourceContext = Parameters<
  typeof selectionSources.describeAxisSource
>[0];
type ThreadTargetContext = Parameters<
  typeof selectionSources.describeThreadTarget
>[0];

interface ActiveThreadedFeaturePanelsProps {
  activeFastenerParameters: FastenerFeatureParameters | null;
  activeFastenerStandards: HoleStandardEntry[];
  activeHelixParameters: HelixFeatureParameters | null;
  activeThreadParameters: ThreadFeatureParameters | null;
  activeThreadStandards: HoleStandardEntry[];
  axisSourceContext: AxisSourceContext;
  disabled: boolean;
  fastenerAction: FastenerAction | null;
  helixAction: HelixAction | null;
  threadAction: ThreadAction | null;
  threadTargetContext: ThreadTargetContext;
  cancelActiveTool: () => Promise<unknown>;
  confirmThread: (featureId: string) => Promise<void>;
  restoreTimelineCursorAfterEdit: () => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  setFastenerAction: (action: FastenerAction | null) => void;
  setHelixAction: (action: HelixAction | null) => void;
  setThreadAction: (action: ThreadAction | null) => void;
  updateActiveFastenerParameters: (
    patch: Partial<FastenerFeatureParameters>,
  ) => Promise<void>;
  updateActiveHelixParameters: (
    patch: Partial<HelixFeatureParameters>,
  ) => Promise<void>;
  updateActiveThreadParameters: (
    patch: Partial<ThreadFeatureParameters>,
  ) => Promise<void>;
}

export function ActiveThreadedFeaturePanels({
  activeFastenerParameters,
  activeFastenerStandards,
  activeHelixParameters,
  activeThreadParameters,
  activeThreadStandards,
  axisSourceContext,
  disabled,
  fastenerAction,
  helixAction,
  threadAction,
  threadTargetContext,
  cancelActiveTool,
  confirmThread,
  restoreTimelineCursorAfterEdit,
  runAction,
  setFastenerAction,
  setHelixAction,
  setThreadAction,
  updateActiveFastenerParameters,
  updateActiveHelixParameters,
  updateActiveThreadParameters,
}: ActiveThreadedFeaturePanelsProps) {
  return (
    <>
      {threadAction ? (
        <ActiveThreadPanel
          action={threadAction}
          axisLabel={
            activeThreadParameters
              ? selectionSources.describeAxisSource(
                  axisSourceContext,
                  activeThreadParameters.axis_source_id,
                )
              : ""
          }
          disabled={disabled}
          parameters={activeThreadParameters}
          pendingAxisLabel={
            threadAction.phase === "pick_target" && threadAction.axisSourceId
              ? selectionSources.describeAxisSource(
                  axisSourceContext,
                  threadAction.axisSourceId,
                )
              : null
          }
          standards={activeThreadStandards}
          targetLabel={
            activeThreadParameters
              ? selectionSources.describeThreadTarget(
                  threadTargetContext,
                  activeThreadParameters.target_body_id,
                )
              : ""
          }
          onCancel={() => {
            void cancelActiveTool();
          }}
          onConfirm={() => {
            if (threadAction.phase !== "active") {
              return;
            }
            void runAction(async () => {
              await confirmThread(threadAction.featureId);
              setThreadAction(null);
              await restoreTimelineCursorAfterEdit();
            });
          }}
          onUpdateParameters={(patch) => {
            void updateActiveThreadParameters(patch);
          }}
        />
      ) : null}
      {fastenerAction && activeFastenerParameters ? (
        <ActiveFastenerPanel
          disabled={disabled}
          parameters={activeFastenerParameters}
          standards={activeFastenerStandards}
          onCancel={() => {
            void cancelActiveTool();
          }}
          onConfirm={() => {
            void runAction(async () => {
              setFastenerAction(null);
              await restoreTimelineCursorAfterEdit();
            });
          }}
          onUpdateParameters={(patch) => {
            void updateActiveFastenerParameters(patch);
          }}
        />
      ) : null}
      {helixAction ? (
        <ActiveHelixPanel
          action={helixAction}
          axisLabel={
            activeHelixParameters
              ? selectionSources.describeAxisSource(
                  axisSourceContext,
                  activeHelixParameters.axis_source_id,
                )
              : ""
          }
          disabled={disabled}
          parameters={activeHelixParameters}
          onCancel={() => {
            void cancelActiveTool();
          }}
          onConfirm={() => setHelixAction(null)}
          onUpdateParameters={(patch) => {
            void updateActiveHelixParameters(patch);
          }}
        />
      ) : null}
    </>
  );
}
