import { BoxFeatureForm, CylinderFeatureForm } from "../layout";
import type { DocumentState } from "../types";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface PrimitiveFeatureEditPanelProps {
  disabled: boolean;
  document: DocumentState | null;
  editingFeatureId: string | null;
  restoreTimelineCursorAfterEdit: () => Promise<unknown>;
  runAction: RunAction;
  setEditingFeatureId: (featureId: string | null) => void;
  updateBoxFeature: (
    featureId: string,
    width: number,
    height: number,
    depth: number,
  ) => Promise<void>;
  updateCylinderFeature: (
    featureId: string,
    radius: number,
    height: number,
  ) => Promise<void>;
}

export function PrimitiveFeatureEditPanel({
  disabled,
  document,
  editingFeatureId,
  restoreTimelineCursorAfterEdit,
  runAction,
  setEditingFeatureId,
  updateBoxFeature,
  updateCylinderFeature,
}: PrimitiveFeatureEditPanelProps) {
  if (!editingFeatureId) {
    return null;
  }

  // Resolve from the live document so the form reflects server-confirmed values.
  const editing = document?.feature_history.find(
    (entry) => entry.feature_id === editingFeatureId,
  );
  if (!editing) {
    queueMicrotask(() => setEditingFeatureId(null));
    return null;
  }

  if (editing.kind === "box" && editing.box_parameters) {
    return (
      <div className="cad-toolbar-popover pointer-events-auto">
        <BoxFeatureForm
          disabled={disabled}
          mode="edit"
          initialValues={{
            width: editing.box_parameters.width,
            height: editing.box_parameters.height,
            depth: editing.box_parameters.depth,
          }}
          variant="toolbar"
          onSubmit={async (width, height, depth) => {
            await runAction(async () => {
              await updateBoxFeature(editingFeatureId, width, height, depth);
            });
            setEditingFeatureId(null);
            await restoreTimelineCursorAfterEdit();
          }}
        />
      </div>
    );
  }

  if (editing.kind === "cylinder" && editing.cylinder_parameters) {
    return (
      <div className="cad-toolbar-popover pointer-events-auto">
        <CylinderFeatureForm
          disabled={disabled}
          mode="edit"
          initialValues={{
            radius: editing.cylinder_parameters.radius,
            height: editing.cylinder_parameters.height,
          }}
          variant="toolbar"
          onSubmit={async (radius, height) => {
            await runAction(async () => {
              await updateCylinderFeature(editingFeatureId, radius, height);
            });
            setEditingFeatureId(null);
            await restoreTimelineCursorAfterEdit();
          }}
        />
      </div>
    );
  }

  return null;
}
