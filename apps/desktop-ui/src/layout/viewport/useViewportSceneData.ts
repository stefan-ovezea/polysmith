import { useEffect, useMemo, useRef } from "react";

import { createViewportScene } from "@/lib";
import type { DocumentState, ViewportState } from "@/types";

type ViewportSceneData = ReturnType<typeof createViewportScene>;

interface ViewportSceneDataContext {
  document: DocumentState | null;
  viewport: ViewportState | null;
  hiddenFeatureIds?: ReadonlySet<string>;
  hiddenSketchPlaneIds?: ReadonlySet<string>;
  hideReferences?: boolean;
}

export function useViewportSceneData({
  document,
  viewport,
  hiddenFeatureIds,
  hiddenSketchPlaneIds,
  hideReferences,
}: ViewportSceneDataContext) {
  const pendingEdgeOpBodyIds = useMemo(() => {
    const result = new Set<string>();
    if (!document) {
      return result;
    }
    for (const feature of document.feature_history) {
      const params =
        feature.fillet_parameters ?? feature.chamfer_parameters ?? null;
      if (params && params.is_pending && params.target_body_id) {
        result.add(params.target_body_id);
      }
    }
    return result;
  }, [document]);

  const sceneData = useMemo<ViewportSceneData | null>(
    () =>
      viewport?.has_active_document
        ? createViewportScene(viewport, {
            hiddenFeatureIds,
            hiddenSketchPlaneIds,
            hideReferences,
            pendingEdgeOpBodyIds,
            document,
          })
        : null,
    [
      viewport,
      hiddenFeatureIds,
      hiddenSketchPlaneIds,
      hideReferences,
      pendingEdgeOpBodyIds,
      document,
    ],
  );

  const sceneDataRef = useRef(sceneData);
  useEffect(() => {
    sceneDataRef.current = sceneData;
  }, [sceneData]);

  return { pendingEdgeOpBodyIds, sceneData, sceneDataRef };
}
