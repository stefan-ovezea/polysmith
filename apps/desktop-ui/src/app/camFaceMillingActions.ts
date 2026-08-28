import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
  FaceAttestation,
  GeometryReference,
  ViewportState,
} from "@/types";

interface CamFaceMillingContext {
  document: DocumentState | null;
  viewport: ViewportState | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  camOperationCreate: (
    operation: CamOperationPayload,
  ) => Promise<Record<string, unknown>>;
  setSelectedOperationId: (operationId: string | null) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

// Builds a FaceAttestation for the currently selected face from the
// viewport's solid-face / body summary data.  The core's witness
// resolution is score-based, so approximate witness data (body bbox +
// face center/normal) is enough to re-identify the face later.
// Returns null when the viewport data is insufficient.
export function buildFaceAttestationFromSelection(
  document: DocumentState,
  viewport: ViewportState | null,
): GeometryReference | null {
  const faceId = document.selected_face_id;
  if (!faceId) {
    return null;
  }
  const face = viewport?.solid_faces.find((entry) => entry.face_id === faceId);
  const body = face
    ? viewport?.bodies.find((entry) => entry.id === face.owner_id)
    : undefined;
  if (!face || !body) {
    return null;
  }

  const halfWidth = body.size.x / 2;
  const halfHeight = body.size.y / 2;
  const halfDepth = body.size.z / 2;
  const minX = body.center.x - halfWidth;
  const maxX = body.center.x + halfWidth;
  const minY = body.center.y - halfHeight;
  const maxY = body.center.y + halfHeight;
  const minZ = body.center.z - halfDepth;
  const maxZ = body.center.z + halfDepth;

  // Four corners of the face's XY bbox at the face's own height.  The
  // witness resolver only needs approximate geometry.
  const faceZ = face.center.z;
  const samplePoints: Array<[number, number, number]> = [
    [minX, minY, faceZ],
    [maxX, minY, faceZ],
    [maxX, maxY, faceZ],
    [minX, maxY, faceZ],
  ];
  const attestation: FaceAttestation = {
    bounds: { min_x: minX, min_y: minY, min_z: minZ, max_x: maxX, max_y: maxY, max_z: maxZ },
    area: (maxX - minX) * (maxY - minY),
    normal: [face.normal.x, face.normal.y, face.normal.z],
    sample_points: samplePoints,
  };
  return {
    persistent_id: faceId,
    attestation,
    fallback_strategy: "warn_user",
  };
}

// Contextual face-milling trigger: requires a selected body face
// (document.selected_face_id).  The face is captured into a
// FaceAttestation built from the viewport's solid-face / body summary
// data.  When the viewport data is insufficient we show an error and
// do not send.
export async function triggerCamFaceMilling({
  document,
  viewport,
  runAction,
  camOperationCreate,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamFaceMillingContext) {
  if (!document) {
    addMessage(translate("cam.faceMilling.noDocument"));
    return;
  }
  if (!document.selected_face_id) {
    addMessage(translate("cam.faceMilling.noSelection"));
    return;
  }
  const reference = buildFaceAttestationFromSelection(document, viewport);
  if (!reference) {
    addMessage(translate("cam.faceMilling.noFaceGeometry"));
    return;
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default endmill on the spot.
  const endmill = document.cam.tool_library.find(
    (tool) => tool.type === "endmill_flat",
  );
  const knownIds = new Set(document.cam.operations.map((op) => op.op_id));

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
    point_locations: [],
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
    const created = operations?.find((op) => !knownIds.has(op.op_id));
    createdId = created?.op_id ?? null;
  });

  if (createdId) {
    setSelectedOperationId(createdId);
  }
}
