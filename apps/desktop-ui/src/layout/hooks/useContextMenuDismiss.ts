import { useEffect } from "react";

interface ContextMenuDismissOptions {
  includeContextMenu?: boolean;
}

export function useContextMenuDismiss(
  isOpen: boolean,
  onDismiss: () => void,
  { includeContextMenu = false }: ContextMenuDismissOptions = {},
) {
  useEffect(() => {
    if (!isOpen) {
      return;
    }
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        onDismiss();
      }
    };
    window.addEventListener("click", onDismiss);
    if (includeContextMenu) {
      window.addEventListener("contextmenu", onDismiss);
    }
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("click", onDismiss);
      if (includeContextMenu) {
        window.removeEventListener("contextmenu", onDismiss);
      }
      window.removeEventListener("keydown", onKey);
    };
  }, [includeContextMenu, isOpen, onDismiss]);
}
