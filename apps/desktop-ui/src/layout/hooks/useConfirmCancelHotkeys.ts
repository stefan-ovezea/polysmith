import { useEffect } from "react";

interface ConfirmCancelHotkeysOptions {
  canConfirm: boolean;
  disabled: boolean;
  onCancel: () => Promise<void>;
  onConfirm: () => void | Promise<void>;
}

export function useConfirmCancelHotkeys({
  canConfirm,
  disabled,
  onCancel,
  onConfirm,
}: ConfirmCancelHotkeysOptions) {
  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      // One Escape = ONE cancel (D7): the App-level listener registers
      // before panel listeners, and same-node listeners all fire
      // regardless of preventDefault — without these guards a single
      // Escape could reach two cancel handlers and consume two undo
      // steps.
      if (event.defaultPrevented) {
        return;
      }
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopImmediatePropagation();
        void onCancel();
      }
      if (event.key === "Enter" && canConfirm && !disabled) {
        event.preventDefault();
        void onConfirm();
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [canConfirm, disabled, onCancel, onConfirm]);
}
