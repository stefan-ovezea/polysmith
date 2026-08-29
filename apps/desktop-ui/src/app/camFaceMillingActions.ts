import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
  FaceAttestation,
  GeometryReference,
} from "@/types";

interface CamFaceMillingContext {
  document: DocumentState | null;
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

// Contextual face-milling trigger: requires a selected body face
// (document.selected_face_id).  The TNP-safe FaceAttestation witness
// is captured by the CORE via cam_capture_face_reference — the UI
// never fabricates witness geometry.
export async function triggerCamFaceMilling({
  document,
  runAction,
  camOperationCreate,
  camCaptureFaceReference,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamFaceMillingContext) {
  if (!document) {
    addMessage(translate("cam.faceMilling.noDocument"));
    return;
  }
  const faceId = document.selected_face_id;
  if (!faceId) {
    addMessage(translate("cam.faceMilling.noSelection"));
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
    addMessage(translate("cam.faceMilling.captureFailed"));
    return;
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default endmill on the spot.
  const endmill = document.cam.tool_library.find(
    (tool) => tool.type === "endmill_flat",
  );
  const previousCount = document.cam.operations.length;

  const operation: CamOperationPayload = {
    name: "Face Mill",
    type: "face_milling",
    enabled: true,
    tool_id: endmill?.tool_id ?? "",
    geometry_references: {
      machining_regions: [reference],
      avoidance_regions: [],
      guide_curves: [],
      check_surfaces: [],
    },
    parameters: {
      spindle_rpm: 8000,
      feedrate_mm_per_min: 1200,
      plunge_feedrate_mm_per_min: 600,
      stepover_percent: 50,
      zigzag_angle_deg: 0,
      stock_allowance_mm: 0.2,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      coolant: "off",
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
