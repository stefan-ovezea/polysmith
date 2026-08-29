import type { CamOperation, CamOperationPayload, DocumentState } from "@/types";
import { DEFAULT_TEST_PATTERN_PARAMS } from "@/layout/CamTestPatternPanel";

interface CamTestPatternContext {
  document: DocumentState | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  camOperationCreate: (
    operation: CamOperationPayload,
  ) => Promise<Record<string, unknown>>;
  setSelectedOperationId: (operationId: string | null) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

// Creates a LightBurn-style material-test card operation: a grid of
// cells sweeping power × speed, engraved or cut.  No geometry
// references — the cells live directly in machine coordinates.
export async function triggerCamTestPattern({
  document,
  runAction,
  camOperationCreate,
  setSelectedOperationId,
  addMessage,
  translate,
}: CamTestPatternContext) {
  if (!document) {
    addMessage(translate("cam.laserCut.noDocument"));
    return;
  }

  const laserTool = document.cam.tool_library.find(
    (tool) => tool.type === "laser",
  );
  const previousCount = document.cam.operations.length;

  const operation: CamOperationPayload = {
    name: "Test Pattern",
    type: "laser_test_pattern",
    enabled: true,
    tool_id: laserTool?.tool_id ?? "",
    parameters: {
      spindle_rpm: 0,
      feedrate_mm_per_min: 500,
      plunge_feedrate_mm_per_min: 0,
      stock_allowance_mm: 0,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      coolant: "off",
      test_pattern: { ...DEFAULT_TEST_PATTERN_PARAMS },
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
  } else {
    addMessage(translate("cam.testPattern.createFailed"));
  }
}
