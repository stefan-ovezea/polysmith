import type { CrosshairMode } from "@/config";
import type { SketchTool } from "@/types";
import { CROSSHAIR_SIZE_FACTORS } from "./viewportPanelTypes";

interface ViewportCrosshairStateContext {
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool;
  crosshairMode: CrosshairMode;
  viewportSize: {
    width: number;
    height: number;
  };
}

export function computeViewportCrosshairState({
  activeSketchPlaneId,
  activeSketchTool,
  crosshairMode,
  viewportSize,
}: ViewportCrosshairStateContext) {
  const isSketchDrawingCursor =
    Boolean(activeSketchPlaneId) &&
    activeSketchTool !== "select" &&
    activeSketchTool !== "project";
  const usesCrosshairGuide =
    crosshairMode === "viewport-25" ||
    crosshairMode === "viewport-50" ||
    crosshairMode === "viewport-75" ||
    crosshairMode === "infinite";
  const crosshairGuideSize =
    crosshairMode === "infinite"
      ? Math.max(viewportSize.width, viewportSize.height) * 2
      : viewportSize.height * (CROSSHAIR_SIZE_FACTORS[crosshairMode] ?? 0) || 18;
  const crosshairCanvasClass = isSketchDrawingCursor
    ? [
        "cad-viewport-canvas-drawing",
      ]
        .filter(Boolean)
        .join(" ")
    : "";

  return {
    crosshairCanvasClass,
    crosshairGuideSize,
    isSketchDrawingCursor,
    usesCrosshairGuide,
  };
}
