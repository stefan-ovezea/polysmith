import type { PointerEvent as ReactPointerEvent } from "react";

export interface SidebarResizerProps {
  width: number;
  onResize: (width: number) => void;
  minWidth?: number;
  maxWidth?: number;
}

export function SidebarResizer({
  width,
  onResize,
  minWidth = 220,
  maxWidth = 640,
}: SidebarResizerProps) {
  function handlePointerDown(event: ReactPointerEvent<HTMLDivElement>) {
    event.preventDefault();
    const startX = event.clientX;
    const startWidth = width;
    const onMove = (moveEvent: PointerEvent) => {
      const next = Math.max(
        minWidth,
        Math.min(maxWidth, startWidth + (moveEvent.clientX - startX)),
      );
      onResize(next);
    };
    const onUp = () => {
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
    };
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
  }

  return (
    <div
      className="cad-sidebar-resizer"
      onPointerDown={handlePointerDown}
    />
  );
}
