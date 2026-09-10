import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
} from "@/types";
import { useToastStore } from "@/state/toastStore";

interface CamDrillingContext {
  document: DocumentState | null;
  // The ACTIVE CAM setup — the new operation joins it.
  setupId: string | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  camOperationCreate: (
    operation: CamOperationPayload,
  ) => Promise<Record<string, unknown>>;
  setSelectedOperationId: (operationId: string | null) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

// Contextual drilling trigger: requires an active CAM setup only —
// the holes are BODY geometry (wall faces, rim edges) or free points,
// never sketch input (STEP/STL imports have no sketch to select).
// The operation is created with empty machining_regions and the holes
// are added afterwards with the armed pick on the panel.
export async function triggerCamDrilling({
  document,
  setupId,
  runAction,
  camOperationCreate,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamDrillingContext) {
  // Failures surface as a toast — addMessage alone lands in the
  // Logs panel, which the user does not see while clicking buttons.
  const pushToast = useToastStore.getState().pushToast;
  if (!document) {
    addMessage(translate("cam.drilling.noDocument"));
    pushToast("warn", translate("cam.drilling.noDocument"));
    return;
  }
  if (!setupId) {
    addMessage(translate("cam.drilling.noSetup"));
    pushToast("warn", translate("cam.drilling.noSetup"));
    return;
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default drill on the spot.
  const drill = document.cam.tool_library.find(
    (tool) => tool.type === "drill",
  );
  const previousCount = document.cam.operations.length;

  // Empty machining_regions — drilling always starts empty; the armed
  // pick fills the holes (wall faces, rim edges, and free points).
  const operation: CamOperationPayload = {
    name: "Drill",
    type: "drilling",
    enabled: true,
    setup_id: setupId ?? "",
    tool_id: drill?.tool_id ?? "",
    geometry_references: {
      machining_regions: [],
      avoidance_regions: [],
      guide_curves: [],
      check_surfaces: [],
    },
    parameters: {
      spindle_rpm: 8000,
      feedrate_mm_per_min: 200,
      plunge_feedrate_mm_per_min: 200,
      stock_allowance_mm: 0,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      cycle_type: "g81_standard",
      hole_depth_mm: 5,
      peck_depth_mm: 2,
      through_hole: false,
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
