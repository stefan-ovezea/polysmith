import type { Dispatch, MutableRefObject, SetStateAction } from "react";
import {
  AnglePlanePanel,
  EdgeOpPreviewPanel,
  MovePreviewPanel,
  OffsetPlanePanel,
  ShellPreviewPanel,
} from "../layout";
import type { MoveFeatureParameters } from "../types";
import type {
  ActiveEdgeOpAction,
  ActiveMoveAction,
  AnglePlaneAction,
  OffsetPlaneAction,
  ShellAction,
} from "./appState";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveBodyOperationPanelsProps {
  activeMoveParameters: MoveFeatureParameters | null;
  activeEdgeIdsRef: MutableRefObject<string[]>;
  anglePlaneAction: AnglePlaneAction | null;
  disabled: boolean;
  edgeOpAction: ActiveEdgeOpAction | null;
  moveAction: ActiveMoveAction | null;
  moveBodyLabel: string | null;
  offsetPlaneAction: OffsetPlaneAction | null;
  pendingAngleRef: MutableRefObject<number>;
  pendingOffsetRef: MutableRefObject<number>;
  pendingShellThicknessRef: MutableRefObject<number>;
  pendingValueRef: MutableRefObject<number>;
  shellAction: ShellAction | null;
  clearSelection: () => Promise<void>;
  confirmChamfer: (featureId: string) => Promise<void>;
  confirmFillet: (featureId: string) => Promise<void>;
  confirmMove: (featureId: string) => Promise<void>;
  confirmShell: (featureId: string) => Promise<void>;
  restoreTimelineCursorAfterEdit: () => Promise<void>;
  runAction: RunAction;
  setAnglePlaneAction: Dispatch<SetStateAction<AnglePlaneAction | null>>;
  setEdgeOpAction: Dispatch<SetStateAction<ActiveEdgeOpAction | null>>;
  setMoveAction: Dispatch<SetStateAction<ActiveMoveAction | null>>;
  setOffsetPlaneAction: Dispatch<SetStateAction<OffsetPlaneAction | null>>;
  setShellAction: Dispatch<SetStateAction<ShellAction | null>>;
  translate: (key: string) => string;
  updateActiveMovePreviewParameters: (
    parameters: MoveFeatureParameters,
  ) => Promise<boolean>;
  updateAnglePlane: (
    featureId: string,
    angleDegrees: number,
  ) => Promise<void>;
  updateChamferDistance: (featureId: string, distance: number) => Promise<void>;
  updateFilletRadius: (featureId: string, radius: number) => Promise<void>;
  updateOffsetPlane: (featureId: string, offset: number) => Promise<void>;
  updateShellThickness: (featureId: string, thickness: number) => Promise<void>;
  onCancelActiveTool: () => Promise<unknown>;
}

export function ActiveBodyOperationPanels({
  activeMoveParameters,
  activeEdgeIdsRef,
  anglePlaneAction,
  disabled,
  edgeOpAction,
  moveAction,
  moveBodyLabel,
  offsetPlaneAction,
  pendingAngleRef,
  pendingOffsetRef,
  pendingShellThicknessRef,
  pendingValueRef,
  shellAction,
  clearSelection,
  confirmChamfer,
  confirmFillet,
  confirmMove,
  confirmShell,
  restoreTimelineCursorAfterEdit,
  runAction,
  setAnglePlaneAction,
  setEdgeOpAction,
  setMoveAction,
  setOffsetPlaneAction,
  setShellAction,
  translate,
  updateActiveMovePreviewParameters,
  updateAnglePlane,
  updateChamferDistance,
  updateFilletRadius,
  updateOffsetPlane,
  updateShellThickness,
  onCancelActiveTool,
}: ActiveBodyOperationPanelsProps) {
  async function cancelActiveTool() {
    await onCancelActiveTool();
  }

  return (
    <>
      {moveAction ? (
        <MovePreviewPanel
          phase={moveAction.phase}
          bodyLabel={moveAction.phase === "active" ? moveBodyLabel : null}
          parameters={
            moveAction.phase === "active"
              ? (activeMoveParameters ?? moveAction.parameters)
              : moveAction.parameters
          }
          disabled={disabled}
          onPreviewParameters={async (parameters) => {
            if (await updateActiveMovePreviewParameters(parameters)) {
              return;
            }
            if (moveAction.phase === "pending") {
              setMoveAction((current) =>
                current?.phase === "pending" ? { ...current, parameters } : current,
              );
            }
          }}
          onConfirm={async () => {
            if (moveAction.phase === "active") {
              await runAction(async () => {
                await confirmMove(moveAction.featureId);
                await clearSelection();
              });
            }
            setMoveAction(null);
            await restoreTimelineCursorAfterEdit();
          }}
          onCancel={cancelActiveTool}
        />
      ) : null}
      {edgeOpAction ? (
        <EdgeOpPreviewPanel
          title={
            edgeOpAction.kind === "fillet"
              ? translate("toolbar.fillet")
              : translate("toolbar.chamfer")
          }
          valueLabel={
            edgeOpAction.kind === "fillet"
              ? translate("forms.radiusMm")
              : translate("forms.distanceMm")
          }
          initialValue={edgeOpAction.initialValue}
          disabled={disabled}
          edgeCount={
            edgeOpAction.phase === "active" ? edgeOpAction.edgeIds.length : 0
          }
          onPreviewValue={async (value) => {
            if (edgeOpAction.phase === "pending") {
              pendingValueRef.current = value;
              return;
            }
            await runAction(async () => {
              if (edgeOpAction.kind === "fillet") {
                await updateFilletRadius(edgeOpAction.featureId, value);
              } else {
                await updateChamferDistance(edgeOpAction.featureId, value);
              }
            });
          }}
          onConfirm={async () => {
            if (edgeOpAction.phase === "active") {
              const featureId = edgeOpAction.featureId;
              const kind = edgeOpAction.kind;
              await runAction(async () => {
                if (kind === "fillet") {
                  await confirmFillet(featureId);
                } else {
                  await confirmChamfer(featureId);
                }
                await clearSelection();
              });
            }
            activeEdgeIdsRef.current = [];
            setEdgeOpAction(null);
          }}
          onCancel={cancelActiveTool}
        />
      ) : null}
      {shellAction ? (
        <ShellPreviewPanel
          isPending={shellAction.phase === "pending"}
          initialThickness={shellAction.initialThickness}
          faceSummary={
            shellAction.phase === "active" ? shellAction.faceSummary : ""
          }
          disabled={disabled}
          onPreviewThickness={async (thickness) => {
            if (shellAction.phase === "pending") {
              pendingShellThicknessRef.current = thickness;
              return;
            }
            await runAction(async () => {
              await updateShellThickness(shellAction.featureId, thickness);
            });
          }}
          onConfirm={async () => {
            if (shellAction.phase === "active") {
              await runAction(async () => {
                await confirmShell(shellAction.featureId);
                await clearSelection();
              });
            }
            setShellAction(null);
          }}
          onCancel={cancelActiveTool}
        />
      ) : null}
      {offsetPlaneAction ? (
        <OffsetPlanePanel
          isPending={offsetPlaneAction.phase === "pending"}
          initialOffset={offsetPlaneAction.initialOffset}
          sourceSummary={
            offsetPlaneAction.phase === "active"
              ? offsetPlaneAction.sourceSummary
              : ""
          }
          disabled={disabled}
          onPreviewOffset={async (offset) => {
            if (offsetPlaneAction.phase === "pending") {
              pendingOffsetRef.current = offset;
              return;
            }
            await runAction(async () => {
              await updateOffsetPlane(offsetPlaneAction.featureId, offset);
            });
          }}
          onConfirm={async () => {
            setOffsetPlaneAction(null);
          }}
          onCancel={cancelActiveTool}
        />
      ) : null}
      {anglePlaneAction ? (
        <AnglePlanePanel
          phase={anglePlaneAction.phase}
          initialAngle={anglePlaneAction.initialAngle}
          sourceSummary={
            anglePlaneAction.phase === "pick_plane"
              ? ""
              : anglePlaneAction.sourceSummary
          }
          axisSummary={
            anglePlaneAction.phase === "active" ? anglePlaneAction.axisSummary : ""
          }
          disabled={disabled}
          onPreviewAngle={async (angleDegrees) => {
            if (anglePlaneAction.phase !== "active") {
              pendingAngleRef.current = angleDegrees;
              return;
            }
            await runAction(async () => {
              await updateAnglePlane(anglePlaneAction.featureId, angleDegrees);
            });
          }}
          onConfirm={async () => {
            setAnglePlaneAction(null);
          }}
          onCancel={cancelActiveTool}
        />
      ) : null}
    </>
  );
}
