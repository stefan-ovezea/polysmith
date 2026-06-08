import type { Dispatch, SetStateAction } from "react";

import {
  LoftPreviewPanel,
  RevolvePreviewPanel,
  SweepPreviewPanel,
} from "../layout";
import type { DocumentState } from "../types/ipc";
import type {
  ActiveLoftAction,
  ActiveRevolveAction,
  ActiveSweepAction,
} from "./appState";

type AsyncVoid = () => Promise<void>;
type AnyAsync = () => Promise<unknown>;
type RunAction = (action: AsyncVoid) => Promise<void>;
type FeatureHistoryEntry = DocumentState["feature_history"][number];

interface ActiveProfileFeaturePanelsProps {
  disabled: boolean;
  loftAction: ActiveLoftAction | null;
  revolveAction: ActiveRevolveAction | null;
  sweepAction: ActiveSweepAction | null;
  selectedSketchProfileIds: string[];
  sketchLineLabelById: Map<string, string>;
  sketchPathEntityLabelById: Map<string, string>;
  sketchProfileLabelById: Map<string, string>;
  hideFeatureSourceSketches: (
    featureId: string,
    readSourceSketchIds: (
      feature: FeatureHistoryEntry,
    ) => Array<string | null | undefined>,
  ) => void;
  runAction: RunAction;
  restoreTimelineCursorAfterEdit: AsyncVoid;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  setLoftAction: Dispatch<SetStateAction<ActiveLoftAction | null>>;
  setRevolveAction: Dispatch<SetStateAction<ActiveRevolveAction | null>>;
  setSweepAction: Dispatch<SetStateAction<ActiveSweepAction | null>>;
  updateLoftRuled: (featureId: string, ruled: boolean) => Promise<void>;
  updateRevolveAngle: (
    featureId: string,
    angleDegrees: number,
  ) => Promise<void>;
  onCancelActiveTool: AnyAsync;
}

export function ActiveProfileFeaturePanels({
  disabled,
  loftAction,
  revolveAction,
  sweepAction,
  selectedSketchProfileIds,
  sketchLineLabelById,
  sketchPathEntityLabelById,
  sketchProfileLabelById,
  hideFeatureSourceSketches,
  runAction,
  restoreTimelineCursorAfterEdit,
  selectSketchProfile,
  setLoftAction,
  setRevolveAction,
  setSweepAction,
  updateLoftRuled,
  updateRevolveAngle,
  onCancelActiveTool,
}: ActiveProfileFeaturePanelsProps) {
  return (
    <>
      {loftAction ? (
        <LoftPreviewPanel
          initialRuled={loftAction.initialRuled}
          profiles={loftAction.profileIds.map((profileId, index) => ({
            profileId,
            label:
              sketchProfileLabelById.get(profileId) ?? `Profile ${index + 1}`,
          }))}
          disabled={disabled}
          canConfirm={
            loftAction.phase === "active" && loftAction.profileIds.length >= 2
          }
          onPreviewRuled={async (ruled) => {
            if (loftAction.phase === "active" && loftAction.featureId) {
              await runAction(async () => {
                await updateLoftRuled(loftAction.featureId!, ruled);
              });
            }
            setLoftAction((current) =>
              current?.featureId === loftAction.featureId
                ? { ...current, initialRuled: ruled }
                : current,
            );
          }}
          onMoveProfile={(profileId, direction) => {
            setLoftAction((current) => {
              if (!current) {
                return current;
              }
              const fromIndex = current.profileIds.indexOf(profileId);
              const toIndex = fromIndex + direction;
              if (
                fromIndex < 0 ||
                toIndex < 0 ||
                toIndex >= current.profileIds.length
              ) {
                return current;
              }
              const nextProfileIds = [...current.profileIds];
              const [moved] = nextProfileIds.splice(fromIndex, 1);
              nextProfileIds.splice(toIndex, 0, moved);
              return { ...current, profileIds: nextProfileIds };
            });
          }}
          onRemoveProfile={async (profileId) => {
            setLoftAction((current) =>
              current
                ? {
                    ...current,
                    profileIds: current.profileIds.filter(
                      (id) => id !== profileId,
                    ),
                  }
                : current,
            );
            if (selectedSketchProfileIds.includes(profileId)) {
              await runAction(async () => {
                await selectSketchProfile(profileId, true);
              });
            }
          }}
          onConfirm={async () => {
            if (loftAction.phase !== "active" || !loftAction.featureId) {
              return;
            }
            hideFeatureSourceSketches(loftAction.featureId, (feature) =>
              feature.loft_parameters?.sections.map(
                (section) => section.sketch_feature_id,
              ) ?? [],
            );
            setLoftAction(null);
            await restoreTimelineCursorAfterEdit();
          }}
          onCancel={async () => {
            await onCancelActiveTool();
          }}
        />
      ) : null}
      {revolveAction ? (
        <RevolvePreviewPanel
          phase={revolveAction.phase}
          initialAngle={revolveAction.initialAngle}
          profileLabel={
            revolveAction.profileId
              ? sketchProfileLabelById.get(revolveAction.profileId) ?? "Profile"
              : null
          }
          axisLabel={
            revolveAction.axisEntityId
              ? sketchLineLabelById.get(revolveAction.axisEntityId) ?? "Line"
              : null
          }
          disabled={disabled}
          canConfirm={
            revolveAction.phase === "active" &&
            Boolean(revolveAction.profileId) &&
            Boolean(revolveAction.axisEntityId)
          }
          onPreviewAngle={async (angleDegrees) => {
            if (revolveAction.phase === "active" && revolveAction.featureId) {
              await runAction(async () => {
                await updateRevolveAngle(
                  revolveAction.featureId!,
                  angleDegrees,
                );
              });
            }
            setRevolveAction((current) =>
              current?.featureId === revolveAction.featureId
                ? { ...current, initialAngle: angleDegrees }
                : current,
            );
          }}
          onConfirm={async (angleDegrees) => {
            if (
              revolveAction.phase !== "active" ||
              !revolveAction.featureId
            ) {
              return;
            }
            await runAction(async () => {
              await updateRevolveAngle(revolveAction.featureId!, angleDegrees);
            });
            hideFeatureSourceSketches(revolveAction.featureId, (feature) => [
              feature.revolve_parameters?.sketch_feature_id,
              feature.revolve_parameters?.axis_sketch_feature_id,
            ]);
            setRevolveAction(null);
            await restoreTimelineCursorAfterEdit();
          }}
          onCancel={async () => {
            await onCancelActiveTool();
          }}
        />
      ) : null}
      {sweepAction ? (
        <SweepPreviewPanel
          phase={sweepAction.phase}
          profileLabel={
            sweepAction.profileId
              ? sketchProfileLabelById.get(sweepAction.profileId) ?? "Profile"
              : null
          }
          pathLabel={
            sweepAction.pathEntityId
              ? sketchPathEntityLabelById.get(sweepAction.pathEntityId) ??
                "Line"
              : null
          }
          disabled={disabled}
          canConfirm={
            sweepAction.phase === "active" &&
            Boolean(sweepAction.profileId) &&
            Boolean(sweepAction.pathEntityId)
          }
          onConfirm={async () => {
            if (sweepAction.phase !== "active" || !sweepAction.featureId) {
              return;
            }
            hideFeatureSourceSketches(sweepAction.featureId, (feature) => [
              feature.sweep_parameters?.sketch_feature_id,
              feature.sweep_parameters?.path_sketch_feature_id,
            ]);
            setSweepAction(null);
            await restoreTimelineCursorAfterEdit();
          }}
          onCancel={async () => {
            await onCancelActiveTool();
          }}
        />
      ) : null}
    </>
  );
}
