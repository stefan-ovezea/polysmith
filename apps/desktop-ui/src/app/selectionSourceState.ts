import type { DocumentState, ViewportState } from "../types";
import type { SketchSourceLabels } from "./sketchSourceLabels";

type Translate = (key: string, options?: Record<string, unknown>) => string;

export interface SelectionSourceStateInput extends SketchSourceLabels {
  document: DocumentState | null;
  viewport: ViewportState | null;
  selectedSketchProfileIds: readonly string[];
  selectedSketchableFaceId: string | null;
  translate: Translate;
}

export function buildSelectionSourceState({
  document,
  viewport,
  selectedSketchProfileIds,
  selectedSketchableFaceId,
  sketchProfileLabelById,
  sketchLineLabelById,
  sketchPathEntityLabelById,
  translate,
}: SelectionSourceStateInput) {
  const planeSourceContext = {
    document,
    viewport,
    selectedSketchProfileIds,
    sketchProfileLabelById,
    translate,
  };
  const axisSourceContext = {
    document,
    viewport,
    sketchLineLabelById,
    translate,
  };

  return {
    planeSourceContext,
    axisSourceContext,
    threadTargetContext: {
      document,
      viewport,
      translate,
    },
    selectedExtrudableFaceId:
      selectedSketchProfileIds.length === 0 ? selectedSketchableFaceId : null,
    selectedSweepPathEntityId: selectedSweepPathEntityId(
      document,
      sketchPathEntityLabelById,
    ),
  };
}

function selectedSweepPathEntityId(
  document: DocumentState | null,
  sketchPathEntityLabelById: ReadonlyMap<string, string>,
) {
  const selectedSketchEntityId = document?.selected_sketch_entity_id;
  if (!selectedSketchEntityId) {
    return null;
  }
  return sketchPathEntityLabelById.has(selectedSketchEntityId)
    ? selectedSketchEntityId
    : null;
}
