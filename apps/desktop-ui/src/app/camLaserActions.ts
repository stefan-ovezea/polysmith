import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
  FaceAttestation,
  GeometryReference,
  ViewportState,
} from "@/types";
import { DEFAULT_LASER_PARAMS } from "@/layout/CamLaserCutPanel";

interface CamLaserCutContext {
  document: DocumentState | null;
  viewport: ViewportState | null;
  // The ACTIVE CAM setup — the new operation joins it.
  setupId: string | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  camOperationCreate: (
    operation: CamOperationPayload,
  ) => Promise<Record<string, unknown>>;
  camCaptureFaceReference: (
    faceId: string,
  ) => Promise<{
    payload?: {
      persistent_id: string;
      attestation: FaceAttestation;
    };
  }>;
  setSelectedOperationId: (operationId: string | null) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

// In the CAM workspace closed sketches are not pickable by default —
// a click on sketch geometry selects the OWNING SKETCH FEATURE, which
// is what the 2D Cut trigger consumes (the core then captures every
// profile of the selected sketch).
export async function selectCamSketchFeature({
  entityId,
  document,
  selectFeature,
  runAction,
  addMessage,
  translate,
}: {
  entityId: string;
  document: DocumentState | null;
  selectFeature: (featureId: string) => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}) {
  if (!document) {
    return;
  }
  const sketch = document.feature_history.find((feature) => {
    if (feature.kind !== "sketch" || !feature.sketch_parameters) {
      return false;
    }
    const params = feature.sketch_parameters;
    return (
      params.lines.some((line) => line.line_id === entityId) ||
      params.circles.some((circle) => circle.circle_id === entityId) ||
      params.arcs.some((arc) => arc.arc_id === entityId)
    );
  });
  if (!sketch) {
    return;
  }
  await runAction(async () => {
    await selectFeature(sketch.feature_id);
  });
  // The CAM sidebar has no visible selection state for features, so
  // confirm the pick through the message log.
  addMessage(
    translate("cam.laserCut.sketchSelected", { name: sketch.name || "Sketch" }),
  );
}

// Contextual 2D-cut trigger: the user selects sketch profiles while
// editing the sketch, OR selects the closed sketch as a feature (the
// CAM-workspace flow), then hits the "2D Cut" toolbar button.  The
// operation is created WITHOUT geometry references — the core captures
// witness references from selected_sketch_profile_ids, falling back to
// every profile of the selected sketch feature.  After the create
// round-trip the fresh operation (core-assigned "cam-op-N" id) is
// selected so its panel opens.
export async function triggerCamLaserCut({
  document,
  viewport,
  setupId,
  runAction,
  camOperationCreate,
  camCaptureFaceReference,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamLaserCutContext) {
  if (!document) {
    addMessage(translate("cam.laserCut.noDocument"));
    return;
  }
  const hasProfileSelection =
    (document.selected_sketch_profile_ids?.length ?? 0) > 0;
  const hasSelectedSketch = document.feature_history.some(
    (feature) =>
      feature.feature_id === document.selected_feature_id &&
      feature.kind === "sketch",
  );

  // Input geometry: sketch profiles (or the selected sketch feature),
  // OR a selected 3D face whose outline gets cut at its height.  The
  // face witness is captured by the CORE — never fabricated here.
  let faceReference: GeometryReference | null = null;
  if (!hasProfileSelection && !hasSelectedSketch && document.selected_face_id) {
    await runAction(async () => {
      const response = await camCaptureFaceReference(
        document.selected_face_id as string,
      );
      const payload = response.payload;
      if (payload?.attestation) {
        faceReference = {
          persistent_id: payload.persistent_id,
          attestation: payload.attestation,
        };
      }
    });
  }
  if (!hasProfileSelection && !hasSelectedSketch && !faceReference) {
    // No pre-selection: the operation is created with an EMPTY scope
    // and the panel's reference-sketch dropdown fills it in afterwards
    // (the core allows empty-scope laser operations on create).
    addMessage(translate("cam.laserCut.scopePickHint"));
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default one on the spot — the very first cut
  // must not require a manual tool-library step.
  const laserTool = document.cam.tool_library.find(
    (tool) => tool.type === "laser",
  );
  const previousCount = document.cam.operations.length;

  const operation: CamOperationPayload = {
    name: "2D Cut",
    type: "laser_cut",
    enabled: true,
    setup_id: setupId ?? "",
    tool_id: laserTool?.tool_id ?? "",
    ...(faceReference
      ? {
          geometry_references: {
            machining_regions: [faceReference] as GeometryReference[],
            avoidance_regions: [],
            guide_curves: [],
            check_surfaces: [],
          },
        }
      : {}),
    parameters: {
      spindle_rpm: 0,
      feedrate_mm_per_min: 500,
      plunge_feedrate_mm_per_min: 0,
      stock_allowance_mm: 0,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      coolant: "off",
      laser: { ...DEFAULT_LASER_PARAMS },
    },
    dependencies: {
      parent_operation_ids: [],
      requires_operation_id: null,
      use_stock_from_previous: false,
    },
    status: "pending",
    status_message: "",
  };

  let createdId: string | null = null;
  await runAction(async () => {
    const response = await camOperationCreate(operation);
    const operations = (
      response as { payload?: { operations?: CamOperation[] } }
    ).payload?.operations;
    // Creation appends — the new operation is the LAST element
    // (guard on length instead of diffing id sets).
    const created =
      (operations?.length ?? 0) === previousCount + 1
        ? operations[operations.length - 1]
        : null;
    createdId = created?.op_id ?? null;
  });

  if (createdId) {
    setSelectedOperationId(createdId);
  }
}
