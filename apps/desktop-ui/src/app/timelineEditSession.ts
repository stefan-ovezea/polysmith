import { useRef, useState } from "react";
import type { Dispatch, SetStateAction } from "react";
import { useCadCoreStore } from "../state";
import type { DocumentState } from "../types/ipc";

type SetTimelineCursor = (includedActionCount: number) => Promise<void>;

export interface TimelineEditSession {
  timelineEditVisibleFeatureIds: Set<string>;
  setTimelineEditVisibleFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  beginTimelineEditSession: (featureId: string, featureKind: string) => void;
  restoreTimelineCursorAfterEdit: () => Promise<void>;
}

export function useTimelineEditSession({
  document,
  setTimelineCursor,
}: {
  document: DocumentState | null;
  setTimelineCursor: SetTimelineCursor;
}): TimelineEditSession {
  const restoreTimelineCursorAfterEditRef = useRef(false);
  const [timelineEditVisibleFeatureIds, setTimelineEditVisibleFeatureIds] =
    useState<Set<string>>(() => new Set<string>());

  function clearTimelineEditVisibility() {
    setTimelineEditVisibleFeatureIds((current) =>
      current.size === 0 ? current : new Set<string>(),
    );
  }

  function beginTimelineEditSession(featureId: string, featureKind: string) {
    restoreTimelineCursorAfterEditRef.current = document?.timeline_cursor === null;
    if (featureKind === "sketch") {
      setTimelineEditVisibleFeatureIds(new Set([featureId]));
      return;
    }
    clearTimelineEditVisibility();
  }

  async function restoreTimelineCursorAfterEdit() {
    clearTimelineEditVisibility();
    if (!restoreTimelineCursorAfterEditRef.current) {
      return;
    }
    restoreTimelineCursorAfterEditRef.current = false;
    const latestDocument = useCadCoreStore.getState().document ?? document;
    const actionCount =
      latestDocument?.feature_history.filter(
        (feature) => feature.kind !== "root_part",
      ).length ?? 0;
    await setTimelineCursor(actionCount);
  }

  return {
    timelineEditVisibleFeatureIds,
    setTimelineEditVisibleFeatureIds,
    beginTimelineEditSession,
    restoreTimelineCursorAfterEdit,
  };
}
