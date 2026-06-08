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
      if (event.key === "Escape") {
        event.preventDefault();
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
