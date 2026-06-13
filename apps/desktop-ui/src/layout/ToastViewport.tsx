import { useEffect } from "react";
import { useTranslation } from "react-i18next";

import type { ToastLevel, ToastNotification } from "@/state";
import { useToastStore } from "@/state";

const TOAST_TIMEOUT_MS = 5000;

const LEVEL_STYLES: Record<ToastLevel, string> = {
  info: "border-primary-bright/50 text-primary-soft",
  warn: "border-tertiary-plane-edge/60 text-tertiary-plane-edge",
  error: "border-danger/60 text-danger",
};

const LEVEL_DOTS: Record<ToastLevel, string> = {
  info: "bg-primary-bright",
  warn: "bg-tertiary-plane-edge",
  error: "bg-danger",
};

export function ToastViewport() {
  const toasts = useToastStore((state) => state.toasts);

  return (
    <div className="pointer-events-none fixed right-4 top-26 z-[120] flex w-[min(24rem,calc(100vw-2rem))] flex-col gap-2">
      {toasts.map((toast) => (
        <ToastItem key={toast.id} toast={toast} />
      ))}
    </div>
  );
}

function ToastItem({ toast }: { toast: ToastNotification }) {
  const { t } = useTranslation();
  const dismissToast = useToastStore((state) => state.dismissToast);

  useEffect(() => {
    const timeout = window.setTimeout(
      () => dismissToast(toast.id),
      TOAST_TIMEOUT_MS,
    );
    return () => window.clearTimeout(timeout);
  }, [dismissToast, toast.id]);

  return (
    <section
      className={`pointer-events-auto rounded-lg border bg-surface-container/95 px-3 py-3 text-sm shadow-[0_18px_46px_rgba(0,0,0,0.42)] backdrop-blur-xl ${LEVEL_STYLES[toast.level]}`}
      role={toast.level === "error" ? "alert" : "status"}
      aria-live={toast.level === "error" ? "assertive" : "polite"}
    >
      <div className="flex items-start gap-3">
        <span
          className={`mt-1 h-2 w-2 shrink-0 rounded-full ${LEVEL_DOTS[toast.level]}`}
          aria-hidden="true"
        />
        <div className="min-w-0 flex-1">
          <p className="text-[10px] font-semibold uppercase tracking-[0.18em]">
            {t(`toast.level.${toast.level}`)}
          </p>
          <p className="mt-1 break-words leading-5 text-on-surface">
            {toast.message}
          </p>
        </div>
        <button
          type="button"
          className="inline-flex h-6 w-6 shrink-0 items-center justify-center rounded-md text-on-surface-dim transition-colors hover:bg-surface-high hover:text-on-surface"
          aria-label={t("toast.dismiss")}
          onClick={() => dismissToast(toast.id)}
        >
          X
        </button>
      </div>
    </section>
  );
}
