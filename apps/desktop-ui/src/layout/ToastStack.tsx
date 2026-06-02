import { useEffect } from "react";
import { useTranslation } from "react-i18next";

import type { ToastNotice } from "@/state/cadCoreStore";

interface ToastStackProps {
  toasts: ToastNotice[];
  onDismiss: (id: string) => void;
}

const TOAST_LIFETIME_MS = 7000;

export function ToastStack({ toasts, onDismiss }: ToastStackProps) {
  const { t } = useTranslation();

  useEffect(() => {
    const timers = toasts.map((toast) =>
      window.setTimeout(() => onDismiss(toast.id), TOAST_LIFETIME_MS),
    );
    return () => {
      timers.forEach((timer) => window.clearTimeout(timer));
    };
  }, [onDismiss, toasts]);

  if (toasts.length === 0) {
    return null;
  }

  return (
    <div className="pointer-events-none fixed right-5 top-20 z-[90] flex w-[min(24rem,calc(100vw-2.5rem))] flex-col gap-3">
      {toasts.map((toast) => (
        <section
          key={toast.id}
          className={
            toast.kind === "error"
              ? "pointer-events-auto cad-floating-panel border-danger/40 px-4 py-3"
              : "pointer-events-auto cad-floating-panel border-tertiary-plane-edge/45 px-4 py-3"
          }
          role="status"
          aria-live="polite"
        >
          <div className="flex items-start gap-3">
            <span
              className={
                toast.kind === "error"
                  ? "mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-danger/15 text-sm font-semibold text-danger"
                  : "mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-tertiary-plane-edge/15 text-sm font-semibold text-tertiary-plane-edge"
              }
              aria-hidden="true"
            >
              {toast.kind === "error" ? "!" : "?"}
            </span>
            <div className="min-w-0 flex-1">
              <p className="cad-kicker">
                {toast.kind === "error"
                  ? t("toasts.error")
                  : t("toasts.warning")}
              </p>
              <p className="mt-1 break-words text-sm leading-5 text-on-surface">
                {toast.message}
              </p>
            </div>
            <button
              type="button"
              className="rounded-md px-2 py-1 text-sm text-on-surface-muted transition-colors hover:bg-surface-high hover:text-on-surface"
              onClick={() => onDismiss(toast.id)}
              aria-label={t("toasts.dismiss")}
            >
              x
            </button>
          </div>
        </section>
      ))}
    </div>
  );
}
