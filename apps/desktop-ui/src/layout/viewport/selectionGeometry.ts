export interface SelectionDrag {
  startX: number;
  startY: number;
  currentX: number;
  currentY: number;
  active: boolean;
}

export interface SelectionRectOverlay {
  left: number;
  top: number;
  width: number;
  height: number;
  visible: boolean;
  direction: "window" | "crossing";
}

interface MutableRef<T> {
  current: T;
}

export function selectionRectOverlayFromDrag(
  drag: SelectionDrag,
): SelectionRectOverlay {
  const left = Math.min(drag.startX, drag.currentX);
  const top = Math.min(drag.startY, drag.currentY);
  const width = Math.abs(drag.currentX - drag.startX);
  const height = Math.abs(drag.currentY - drag.startY);
  return {
    left,
    top,
    width,
    height,
    visible: width > 3 || height > 3,
    direction: drag.currentX >= drag.startX ? "window" : "crossing",
  };
}

export function finishRectangleSelectionDrag({
  event,
  selectionDragRef,
  setSelectionRect,
  controls,
  performRectangleSelect,
}: {
  event: PointerEvent;
  selectionDragRef: MutableRef<SelectionDrag | null>;
  setSelectionRect: (overlay: SelectionRectOverlay | null) => void;
  controls: { enabled: boolean };
  performRectangleSelect: (
    drag: SelectionDrag,
    additive: boolean,
  ) => Promise<void>;
}) {
  const drag = selectionDragRef.current;
  if (!drag?.active) {
    return false;
  }

  const width = Math.abs(drag.currentX - drag.startX);
  const height = Math.abs(drag.currentY - drag.startY);
  const wasRealDrag = width > 3 || height > 3;
  if (wasRealDrag) {
    // No modifier = replace the selection; Ctrl/Cmd (or Shift) adds.
    void performRectangleSelect(
      drag,
      event.shiftKey || event.ctrlKey || event.metaKey,
    );
  }
  selectionDragRef.current = null;
  setSelectionRect(null);
  controls.enabled = true;
  // Only consume the event if there was an actual drag. A simple click
  // on empty canvas should propagate so the selection flow can deselect.
  return wasRealDrag;
}
