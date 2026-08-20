import type { Dispatch, SetStateAction } from "react";
import { SketchTextPanel, type SketchTextPanelValue } from "../layout";
import type { SketchTool } from "../types";
import type { SketchTextAction } from "./sketchToolLifecycleEffects";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveSketchTextPanelProps {
  action: SketchTextAction;
  disabled: boolean;
  setSketchTextAction: Dispatch<SetStateAction<SketchTextAction | null>>;
  runAction: RunAction;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  updateSketchText: (
    textId: string,
    patch: {
      text?: string;
      fontPath?: string;
      heightMm?: number;
      angleDeg?: number;
      anchorX?: number;
      anchorY?: number;
      hAlign?: "left" | "center" | "right";
      vAlign?: "top" | "middle" | "bottom";
      charSpacing?: number;
      pathEntityId?: string | null;
      pathOffset?: number;
    },
  ) => Promise<void>;
  deleteSketchText: (textId: string) => Promise<void>;
  // Path picker: armed while the next viewport click should bind a
  // sketch line/arc as the text path (click handling lives in
  // App/viewport); clearing unbinds the current path.
  pathPicking: boolean;
  onArmPathPick: () => void;
  onClearPath: () => void;
}

// contextual modeling state machine for the sketch Text tool,
// mirroring `ActiveSketchFilletPanel` one-to-one:
//   - pending: the panel prompts "click to place text"; a viewport
//     click creates the text (core defaults) and App flips the action
//     to active, bound to the new text id.
//   - active: the panel edits are debounced full-parameter
//     `update_sketch_text` calls; Confirm returns to Select (the text
//     stays committed); Cancel deletes the bound text and returns to
//     Select.
// The panel is remounted per bound text (`key`), so its local state
// initializes from `action.params` exactly once per session.
export function ActiveSketchTextPanel({
  action,
  disabled,
  setSketchTextAction,
  runAction,
  setSketchTool,
  updateSketchText,
  deleteSketchText,
  pathPicking,
  onArmPathPick,
  onClearPath,
}: ActiveSketchTextPanelProps) {
  const isActive = action.phase === "active";
  const params = isActive ? action.params : null;

  return (
    <SketchTextPanel
      key={isActive ? action.textId : "pending"}
      pending={!isActive}
      initialValue={params}
      disabled={disabled}
      pathPicking={pathPicking}
      onArmPathPick={onArmPathPick}
      onClearPath={onClearPath}
      onPreviewValue={async (value: SketchTextPanelValue) => {
        // Keep the action's params in sync so a later Cancel / re-pick
        // sees the latest edits even before the core round-trip lands.
        setSketchTextAction((prev) =>
          prev && prev.phase === "active"
            ? { ...prev, params: { ...prev.params, ...value } }
            : prev,
        );
        if (action.phase !== "active") {
          return;
        }
        await runAction(async () => {
          await updateSketchText(action.textId, {
            text: value.text,
            fontPath: value.font_path,
            heightMm: value.height_mm,
            angleDeg: value.angle_deg,
            hAlign: value.h_align,
            vAlign: value.v_align,
            charSpacing: value.char_spacing,
            pathOffset: value.path_offset,
          });
        });
      }}
      onConfirm={async () => {
        setSketchTextAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
      onCancel={async () => {
        if (action.phase === "active") {
          await runAction(async () => {
            await deleteSketchText(action.textId);
          });
        }
        setSketchTextAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
    />
  );
}
