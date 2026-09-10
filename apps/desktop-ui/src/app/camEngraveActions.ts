import type {
  CamOperation,
  CamOperationPayload,
  DocumentState,
} from "@/types";
import { useToastStore } from "@/state/toastStore";
import { DEFAULT_ENGRAVE_PARAMS } from "@/layout/CamEngravePanel";

interface CamEngraveContext {
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

// Contextual mill-engrave trigger: sketch profiles only (never a body
// face — engraving traces sketch geometry on-line).  The operation is
// created WITHOUT geometry references and the core captures witness
// references from the selection (the same flow as the contour profile
// path — the create gate is widened for "engrave").
export async function triggerCamEngrave({
  document,
  setupId,
  runAction,
  camOperationCreate,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamEngraveContext) {
  // Failures surface as a toast — addMessage alone lands in the
  // Logs panel, which the user does not see while clicking buttons.
  const pushToast = useToastStore.getState().pushToast;
  if (!document) {
    addMessage(translate("cam.engrave.noDocument"));
    pushToast("warn", translate("cam.engrave.noDocument"));
    return;
  }
  if (!setupId) {
    addMessage(translate("cam.engrave.noSetup"));
    pushToast("warn", translate("cam.engrave.noSetup"));
    return;
  }
  const hasProfileSelection =
    (document.selected_sketch_profile_ids?.length ?? 0) > 0;
  const hasSelectedSketch = document.feature_history.some(
    (feature) =>
      feature.feature_id === document.selected_feature_id &&
      feature.kind === "sketch",
  );
  if (!hasProfileSelection && !hasSelectedSketch) {
    addMessage(translate("cam.engrave.noSelection"));
    pushToast("warn", translate("cam.engrave.noSelection"));
    return;
  }

  // The core resolves an empty tool_id: it reuses a matching library
  // tool or creates a default endmill on the spot.
  const endmill = document.cam.tool_library.find(
    (tool) => tool.type === "endmill_flat",
  );
  const previousCount = document.cam.operations.length;

  // NO geometry_references — the core captures selected sketch
  // profiles on create (the widened cam_operation_create gate).
  const operation: CamOperationPayload = {
    name: "Engrave",
    type: "engrave",
    enabled: true,
    setup_id: setupId ?? "",
    tool_id: endmill?.tool_id ?? "",
    parameters: {
      spindle_rpm: 8000,
      feedrate_mm_per_min: 1200,
      plunge_feedrate_mm_per_min: 600,
      stock_allowance_mm: 0,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      engrave: { ...DEFAULT_ENGRAVE_PARAMS },
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
