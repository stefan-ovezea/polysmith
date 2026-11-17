import { useState } from "react";
import type { Dispatch, SetStateAction } from "react";
import type { DocumentState } from "../types/ipc";

type SetTimelineCursor = (includedActionCount: number) => Promise<void>;

/**
 * "Show the whole history" sentinel: any count at or past the last action is
 * clamped by the core to "all".
 *
 * It MUST fit in a 32-bit int. The core reads the count with
 * `json::get<int>()` (`read_int`), so `Number.MAX_SAFE_INTEGER` truncates to
 * -1 and `std::max(0, std::min(-1, actions))` clamps it to 0 — the marker then
 * jumped to the START of the timeline after every operation, hiding every body
 * ("I drag it back and all is ok. Every operation sends the cursor to the
 * beginning").
 */
export const TIMELINE_END_ACTION_COUNT = 2_147_483_647;


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
  const [timelineEditVisibleFeatureIds, setTimelineEditVisibleFeatureIds] =
    useState<Set<string>>(() => new Set<string>());

  function clearTimelineEditVisibility() {
    setTimelineEditVisibleFeatureIds((current) =>
      current.size === 0 ? current : new Set<string>(),
    );
  }

  function beginTimelineEditSession(featureId: string, featureKind: string) {
    if (featureKind === "sketch") {
      setTimelineEditVisibleFeatureIds(new Set([featureId]));
      return;
    }
    clearTimelineEditVisibility();
  }

  async function restoreTimelineCursorAfterEdit() {
    clearTimelineEditVisibility();
    // Ask for the END, never for `feature_history.length` read from the store.
    //
    // Command replies arrive asynchronously, so right after an edit that CREATED
    // a feature the store's document is one feature short; the count then lands
    // the cursor one short, leaving the newest feature beyond it — listed in the
    // hierarchy, absent from the viewport, and with no marker in sight
    // ("I edited the extrusion and again one body and the cursor are not
    // visible"). The core clamps anything past its action count to "all", so the
    // sentinel is exact regardless of what the store knows.
    await setTimelineCursor(TIMELINE_END_ACTION_COUNT);
  }

  return {
    timelineEditVisibleFeatureIds,
    setTimelineEditVisibleFeatureIds,
    beginTimelineEditSession,
    restoreTimelineCursorAfterEdit,
  };
}
