import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
  FaceAttestation,
  GeometryReference,
} from "@/types";
import { useToastStore } from "@/state/toastStore";

interface CamAdaptiveContext {
  document: DocumentState | null;
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

// Contextual Adaptive Clearing trigger: requires a selected body face
// (the area to clear).  Same witness capture flow as the pocket —
// islands are added later through the operation panel's pick flow.
export async function triggerCamAdaptive({
  document,
  setupId,
  runAction,
  camOperationCreate,
  camCaptureFaceReference,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamAdaptiveContext) {
  // Failures surface as a toast — addMessage alone lands in the
  // Logs panel, which the user does not see while clicking buttons.
  const pushToast = useToastStore.getState().pushToast;
  if (!document) {
    addMessage(translate("cam.adaptive.noDocument"));
    pushToast("warn", translate("cam.adaptive.noDocument"));
    return;
  }
  const faceId = document.selected_face_id;
  if (!faceId) {
    addMessage(translate("cam.adaptive.noSelection"));
    pushToast("warn", translate("cam.adaptive.noSelection"));
    return;
  }

  let reference: GeometryReference | null = null;
  await runAction(async () => {
    const response = await camCaptureFaceReference(faceId);
    const payload = response.payload;
    if (payload?.attestation) {
      reference = {
        persistent_id: payload.persistent_id,
        attestation: payload.attestation,
      };
    }
  });
  if (!reference) {
    addMessage(translate("cam.adaptive.captureFailed"));
    pushToast("error", translate("cam.adaptive.captureFailed"));
    return;
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default endmill on the spot.
  const endmill = document.cam.tool_library.find(
    (tool) => tool.type === "endmill_flat",
  );
  const previousCount = document.cam.operations.length;

  const operation: CamOperationPayload = {
    name: "Adaptive Clearing",
    type: "adaptive_clearing",
    enabled: true,
    setup_id: setupId ?? "",
    tool_id: endmill?.tool_id ?? "",
    geometry_references: {
      machining_regions: [reference],
      avoidance_regions: [],
      guide_curves: [],
      check_surfaces: [],
    },
    // Same defaults as the pocket; no default stepdown — a cleared
    // stepdown means single-pass (the face level only).  The v1
    // toolpath is the contour-parallel spiral; engagement_angle_deg
    // and strategy are reserved for the future trochoidal upgrade and
    // stay unset (core defaults apply).
    parameters: {
      spindle_rpm: 8000,
      feedrate_mm_per_min: 1200,
      plunge_feedrate_mm_per_min: 600,
      stepover_percent: 50,
      stock_allowance_mm: 0.2,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      coolant: "off",
      tool_axis_mode: "fixed_z",
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
