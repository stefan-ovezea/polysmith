import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
  EdgeAttestation,
  GeometryReference,
} from "@/types";
import { useToastStore } from "@/state/toastStore";

interface CamSlotContext {
  document: DocumentState | null;
  // The ACTIVE CAM setup — the new operation joins it.
  setupId: string | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  camOperationCreate: (
    operation: CamOperationPayload,
  ) => Promise<Record<string, unknown>>;
  camCaptureEdgeReference: (
    edgeId: string,
  ) => Promise<{
    payload?: {
      persistent_id: string;
      attestation: EdgeAttestation;
    };
  }>;
  setSelectedOperationId: (operationId: string | null) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

// Contextual open-slot trigger: requires an active setup plus one or
// more selected STRAIGHT edges — each is a slot's open side and the
// tool cuts a tool-width groove from the edge INTO the material on
// the side of the adjacent horizontal top face (the core re-derives
// the side from the live body at generate time).  Every selected edge
// is captured as a TNP-safe witness at creation; Re-pick re-captures
// the current selection.
export async function triggerCamSlot({
  document,
  setupId,
  runAction,
  camOperationCreate,
  camCaptureEdgeReference,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamSlotContext) {
  // Failures surface as a toast — addMessage alone lands in the
  // Logs panel, which the user does not see while clicking buttons.
  const pushToast = useToastStore.getState().pushToast;
  if (!document) {
    addMessage(translate("cam.slot.noDocument"));
    pushToast("warn", translate("cam.slot.noDocument"));
    return;
  }
  if (!setupId) {
    addMessage(translate("cam.slot.noSetup"));
    pushToast("warn", translate("cam.slot.noSetup"));
    return;
  }
  const edgeIds = document.selected_edge_ids ?? [];
  if (edgeIds.length === 0) {
    addMessage(translate("cam.slot.noSelection"));
    pushToast("warn", translate("cam.slot.noSelection"));
    return;
  }

  // Capture every selected edge as a witness — the same flow the
  // drilling pick uses, driven by the current selection instead of an
  // armed pick.
  const references: GeometryReference[] = [];
  for (const edgeId of edgeIds) {
    let reference: GeometryReference | null = null;
    await runAction(async () => {
      const response = await camCaptureEdgeReference(edgeId);
      const payload = response.payload;
      if (payload?.attestation) {
        reference = {
          persistent_id: payload.persistent_id,
          attestation: payload.attestation,
        };
      }
    });
    if (!reference) {
      addMessage(translate("cam.slot.captureFailed"));
      pushToast("error", translate("cam.slot.captureFailed"));
      return;
    }
    references.push(reference);
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default endmill on the spot.
  const endmill = document.cam.tool_library.find(
    (tool) => tool.type === "endmill_flat",
  );
  const previousCount = document.cam.operations.length;

  const operation: CamOperationPayload = {
    name: "Slot",
    type: "slot",
    enabled: true,
    setup_id: setupId ?? "",
    tool_id: endmill?.tool_id ?? "",
    geometry_references: {
      machining_regions: references,
      avoidance_regions: [],
      guide_curves: [],
      check_surfaces: [],
    },
    // No default stepdown — a cleared stepdown means a single pass at
    // the face level.  The tool-width groove is the tool diameter;
    // wider slots are a future refinement.
    parameters: {
      spindle_rpm: 8000,
      feedrate_mm_per_min: 1200,
      plunge_feedrate_mm_per_min: 600,
      stock_allowance_mm: 0,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      slot: {
        depth_mm: 5,
      },
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
