import type { ChangeEvent, MutableRefObject } from "react";

import type { HoleFeatureParameters } from "../types";
import type { DocumentState, ViewportState } from "../types/ipc";
import {
  DEFAULT_CHAMFER_DISTANCE,
  DEFAULT_FILLET_RADIUS,
  DEFAULT_HOLE_DEPTH,
  DEFAULT_HOLE_DIAMETER,
  DEFAULT_SHELL_THICKNESS,
  type ActiveEdgeOpAction,
  type HoleAction,
  type ShellAction,
} from "./appState";
import {
  type AppToolState,
  isToolStartBlocked,
} from "./actionAvailability";
import { awaitCreatedFeatureOfKind } from "./featureCreation";
import * as selectionSources from "./selectionSources";

type PlaneSourceContext = Parameters<
  typeof selectionSources.describePlaneSource
>[0];
type HoleCenter = ViewportState["solid_faces"][number]["center"];

interface BodyModifierActionContext {
  activeToolState: AppToolState;
  document: DocumentState | null;
  viewport: ViewportState | null;
  planeSourceContext: PlaneSourceContext;
  pendingValueRef: MutableRefObject<number>;
  activeEdgeIdsRef: MutableRefObject<string[]>;
  pendingShellThicknessRef: MutableRefObject<number>;
  setEdgeOpAction: (action: ActiveEdgeOpAction | null) => void;
  setShellAction: (action: ShellAction | null) => void;
  setHoleAction: (action: HoleAction | null) => void;
  createFillet: (edgeIds: string[], radius: number) => Promise<void>;
  createChamfer: (edgeIds: string[], distance: number) => Promise<void>;
  createShell: (faceId: string, thickness: number) => Promise<void>;
  createHole: (
    faceId: string,
    center: HoleCenter,
    parameters: Partial<HoleFeatureParameters>,
  ) => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
}

export function createBodyModifierActions({
  activeToolState,
  document,
  viewport,
  planeSourceContext,
  pendingValueRef,
  activeEdgeIdsRef,
  pendingShellThicknessRef,
  setEdgeOpAction,
  setShellAction,
  setHoleAction,
  createFillet,
  createChamfer,
  createShell,
  createHole,
  runAction,
  addMessage,
}: BodyModifierActionContext) {
  async function triggerEdgeOpAction(kind: "fillet" | "chamfer") {
    if (isToolStartBlocked(activeToolState)) {
      return;
    }
    const initialValue =
      kind === "fillet" ? DEFAULT_FILLET_RADIUS : DEFAULT_CHAMFER_DISTANCE;
    pendingValueRef.current = initialValue;

    const preSelectedEdgeIds = document?.selected_edge_ids ?? [];
    if (preSelectedEdgeIds.length === 0) {
      setEdgeOpAction({ phase: "pending", kind, initialValue });
      return;
    }

    await createEdgeOpFeature(kind, preSelectedEdgeIds, initialValue);
  }

  async function createEdgeOpFeature(
    kind: "fillet" | "chamfer",
    edgeIds: string[],
    value: number,
  ) {
    if (edgeIds.length === 0) {
      return;
    }
    const documentPromise = awaitCreatedFeatureOfKind(kind);

    await runAction(async () => {
      if (kind === "fillet") {
        await createFillet(edgeIds, value);
      } else {
        await createChamfer(edgeIds, value);
      }
      try {
        const { featureId: newFeatureId } = await documentPromise;
        activeEdgeIdsRef.current = [...edgeIds];
        setEdgeOpAction({
          phase: "active",
          kind,
          featureId: newFeatureId,
          initialValue: value,
          edgeIds: [...edgeIds],
        });
      } catch (error) {
        addMessage(`${kind} action error: ${String(error)}`);
      }
    });
  }

  async function createShellFeature(faceId: string, thickness: number) {
    const documentPromise = awaitCreatedFeatureOfKind("shell");

    await runAction(async () => {
      await createShell(faceId, thickness);
      try {
        const { featureId: newFeatureId } = await documentPromise;
        setShellAction({
          phase: "active",
          featureId: newFeatureId,
          faceId,
          faceSummary: selectionSources.describePlaneSource(
            planeSourceContext,
            faceId,
          ),
          initialThickness: thickness,
        });
      } catch (error) {
        addMessage(`shell action error: ${String(error)}`);
      }
    });
  }

  async function triggerShellAction() {
    if (
      isToolStartBlocked(activeToolState, {
        fastener: false,
        move: false,
      })
    ) {
      return;
    }
    pendingShellThicknessRef.current = DEFAULT_SHELL_THICKNESS;
    const selectedFaceId = document?.selected_face_id ?? null;
    if (selectedFaceId) {
      await createShellFeature(selectedFaceId, DEFAULT_SHELL_THICKNESS);
      return;
    }
    setShellAction({
      phase: "pending",
      initialThickness: DEFAULT_SHELL_THICKNESS,
    });
  }

  async function createHoleFeature(
    faceId: string,
    parameters: Partial<HoleFeatureParameters> = {},
  ) {
    const face = viewport?.solid_faces.find((entry) => entry.face_id === faceId);
    if (!face) {
      addMessage("hole action error: selected face is no longer available");
      return;
    }
    const documentPromise = awaitCreatedFeatureOfKind("hole");

    await runAction(async () => {
      await createHole(faceId, face.center, {
        hole_type: "simple",
        extent_type: "blind",
        diameter: DEFAULT_HOLE_DIAMETER,
        depth: DEFAULT_HOLE_DEPTH,
        counterbore_diameter: DEFAULT_HOLE_DIAMETER * 1.6,
        counterbore_depth: 2,
        countersink_diameter: DEFAULT_HOLE_DIAMETER * 1.6,
        countersink_angle_degrees: 82,
        standard: "custom",
        standard_size: "",
        hole_fit: "clearance",
        thread_enabled: false,
        thread_spec: "",
        thread_pitch: 0,
        major_diameter: 0,
        minor_diameter: 0,
        thread_depth: DEFAULT_HOLE_DEPTH,
        thread_representation: "cosmetic",
        ...parameters,
      });
      try {
        const { featureId: newFeatureId } = await documentPromise;
        setHoleAction({ phase: "active", featureId: newFeatureId });
      } catch (error) {
        addMessage(`hole action error: ${String(error)}`);
      }
    });
  }

  async function triggerHoleAction() {
    if (
      isToolStartBlocked(activeToolState, {
        thread: false,
        fastener: false,
        move: false,
      })
    ) {
      return;
    }
    const selectedFaceId = document?.selected_face_id ?? null;
    if (selectedFaceId) {
      await createHoleFeature(selectedFaceId);
      return;
    }
    setHoleAction({ phase: "pending" });
  }

  return {
    createEdgeOpFeature,
    createHoleFeature,
    createShellFeature,
    triggerEdgeOpAction,
    triggerHoleAction,
    triggerShellAction,
  };
}

interface HoleParameterHandlerContext {
  activeHoleParameters: HoleFeatureParameters | null;
  holeAction: HoleAction | null;
  updateHoleParameters: (
    featureId: string,
    parameters: HoleFeatureParameters,
  ) => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
}

export function createHoleParameterHandlers({
  activeHoleParameters,
  holeAction,
  updateHoleParameters,
  runAction,
}: HoleParameterHandlerContext) {
  function updateActiveHoleParameters(patch: Partial<HoleFeatureParameters>) {
    if (!activeHoleParameters) {
      return;
    }
    void runAction(async () => {
      await updateHoleParameters(
        holeAction?.phase === "active" ? holeAction.featureId : "",
        {
          ...activeHoleParameters,
          ...patch,
        },
      );
    });
  }

  function makePositiveHoleNumberChange(
    patchFromValue: (value: number) => Partial<HoleFeatureParameters>,
  ) {
    return (event: ChangeEvent<HTMLInputElement>) => {
      const value = Number(event.target.value);
      if (!Number.isFinite(value) || value <= 0) {
        return;
      }
      updateActiveHoleParameters(patchFromValue(value));
    };
  }

  return {
    makePositiveHoleNumberChange,
    updateActiveHoleParameters,
  };
}
