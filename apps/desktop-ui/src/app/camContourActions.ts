import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
  FaceAttestation,
  GeometryReference,
  ViewportState,
} from "@/types";
import { useToastStore } from "@/state/toastStore";
import { DEFAULT_CONTOUR_PARAMS } from "@/layout/CamContourPanel";

interface CamContourContext {
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

// Contextual 2D-contour trigger.  Two inputs, face wins over profiles
// (the core's precedence rule):
//   - a selected body face → TNP witness captured here and passed as
//     the machining region (same flow as the pocket trigger);
//   - else selected sketch profiles (or the selected sketch feature) →
//     the operation is created WITHOUT geometry references and the core
//     captures witness references from the selection (same flow as the
//     2D Cut trigger).
export async function triggerCamContour({
  document,
  viewport,
  setupId,
  runAction,
  camOperationCreate,
  camCaptureFaceReference,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamContourContext) {
  // Failures surface as a toast — addMessage alone lands in the
  // Logs panel, which the user does not see while clicking buttons.
  const pushToast = useToastStore.getState().pushToast;
  if (!document) {
    addMessage(translate("cam.contour.noDocument"));
    pushToast("warn", translate("cam.contour.noDocument"));
    return;
  }
  const hasProfileSelection =
    (document.selected_sketch_profile_ids?.length ?? 0) > 0;
  const hasSelectedSketch = document.feature_history.some(
    (feature) =>
      feature.feature_id === document.selected_feature_id &&
      feature.kind === "sketch",
  );
  if (!document.selected_face_id && !hasProfileSelection && !hasSelectedSketch) {
    addMessage(translate("cam.contour.noSelection"));
    pushToast("warn", translate("cam.contour.noSelection"));
    return;
  }

  // Face wins — witness captured by the core capture command, never
  // fabricated here.  The profile fallback needs no capture at all:
  // the core captures selected_sketch_profile_ids on create.
  let faceReference: GeometryReference | null = null;
  if (document.selected_face_id) {
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
    if (!faceReference) {
      addMessage(translate("cam.contour.captureFailed"));
      pushToast("error", translate("cam.contour.captureFailed"));
      return;
    }
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default endmill on the spot.
  const endmill = document.cam.tool_library.find(
    (tool) => tool.type === "endmill_flat",
  );
  const previousCount = document.cam.operations.length;

  const operation: CamOperationPayload = {
    name: "2D Contour",
    type: "contour_2d",
    enabled: true,
    setup_id: setupId ?? "",
    tool_id: endmill?.tool_id ?? "",
    ...(faceReference
      ? {
          geometry_references: {
            machining_regions: [faceReference],
            avoidance_regions: [],
            guide_curves: [],
            check_surfaces: [],
          },
        }
      : {}),
    parameters: {
      spindle_rpm: 8000,
      feedrate_mm_per_min: 1200,
      plunge_feedrate_mm_per_min: 600,
      // 0: the contour generator reads ONLY the contour block's
      // allowance — the shared base field must not drift (risk #4).
      stock_allowance_mm: 0,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      coolant: "off",
      tool_axis_mode: "fixed_z",
      contour: { ...DEFAULT_CONTOUR_PARAMS },
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
    // Creation appends — the new operation is the LAST element.
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
