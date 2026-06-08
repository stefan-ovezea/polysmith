import { useEffect } from "react";
import type { MutableRefObject } from "react";
import { matchesHotkey } from "@/config";
import type { AppConfig, HotkeyBinding } from "@/config";

type UpdateConfig = (updater: (config: AppConfig) => AppConfig) => void;

function isTypingTarget(target: EventTarget | null) {
  return (
    target instanceof HTMLInputElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLSelectElement ||
    (target instanceof HTMLElement && target.isContentEditable)
  );
}

export function useViewportGridHotkey({
  toggleGridHotkey,
  activeSketchPlaneIdRef,
  updateConfig,
}: {
  toggleGridHotkey: HotkeyBinding;
  activeSketchPlaneIdRef: MutableRefObject<string | null>;
  updateConfig: UpdateConfig;
}) {
  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (isTypingTarget(event.target) || !matchesHotkey(event, toggleGridHotkey)) {
        return;
      }
      event.preventDefault();
      updateConfig((current) => {
        const isSketchMode = Boolean(activeSketchPlaneIdRef.current);
        return {
          ...current,
          viewport: {
            ...current.viewport,
            showGrid: isSketchMode
              ? current.viewport.showGrid
              : !current.viewport.showGrid,
            showSketchGrid: isSketchMode
              ? !current.viewport.showSketchGrid
              : current.viewport.showSketchGrid,
          },
        };
      });
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [toggleGridHotkey, updateConfig]);
}

export function useAltSnapOverride(altHeldRef: MutableRefObject<boolean>) {
  useEffect(() => {
    function handleAltDown(event: KeyboardEvent) {
      if (event.key === "Alt") {
        altHeldRef.current = true;
      }
    }
    function handleAltUp(event: KeyboardEvent) {
      if (event.key === "Alt") {
        altHeldRef.current = false;
      }
    }
    window.addEventListener("keydown", handleAltDown);
    window.addEventListener("keyup", handleAltUp);
    return () => {
      window.removeEventListener("keydown", handleAltDown);
      window.removeEventListener("keyup", handleAltUp);
    };
  }, [altHeldRef]);
}

export function useGhostEdgeRevealHotkey({
  pendingEdgeOpBodyIds,
  revealGhostEdgesRef,
  hoveredEdgeIdRef,
  paintEdgeMaterials,
}: {
  pendingEdgeOpBodyIds: ReadonlySet<string>;
  revealGhostEdgesRef: MutableRefObject<boolean>;
  hoveredEdgeIdRef: MutableRefObject<string | null>;
  paintEdgeMaterials: (hoveredEdgeId: string | null) => void;
}) {
  useEffect(() => {
    if (pendingEdgeOpBodyIds.size === 0) {
      return;
    }
    function setReveal(next: boolean) {
      if (revealGhostEdgesRef.current === next) {
        return;
      }
      revealGhostEdgesRef.current = next;
      paintEdgeMaterials(hoveredEdgeIdRef.current);
    }
    function handleKeyDown(event: KeyboardEvent) {
      if (event.code !== "Tab") {
        return;
      }
      if (isTypingTarget(event.target)) {
        return;
      }
      event.preventDefault();
      setReveal(true);
    }
    function handleKeyUp(event: KeyboardEvent) {
      if (event.code !== "Tab") {
        return;
      }
      setReveal(false);
    }
    function handleBlur() {
      setReveal(false);
    }
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    window.addEventListener("blur", handleBlur);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("keyup", handleKeyUp);
      window.removeEventListener("blur", handleBlur);
      revealGhostEdgesRef.current = false;
    };
  }, [pendingEdgeOpBodyIds]);
}
