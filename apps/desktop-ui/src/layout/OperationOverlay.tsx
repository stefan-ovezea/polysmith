import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

import { useCadCoreStore } from "@/state";
import { elapsedSeconds } from "@/utils/operationProgress";

// A veil over the window while a long core command runs.
//
// The core is a separate process but the UI waits on its reply, and a STEP
// assembly import takes ~16 s (transfer, split per solid, then a 40 MB
// viewport payload). Without this the window simply stops repainting and the
// user is left guessing whether the import is running or the app has hung.
//
// It deliberately offers no cancel: the command cannot be interrupted mid
// transfer, and a button that does nothing is worse than no button. What it
// does offer is the stage the core last logged (its log events stream in
// while the command is still running) and a live elapsed count, which is the
// difference between "working" and "frozen".
export function OperationOverlay() {
  const { t } = useTranslation();
  const busy = useCadCoreStore((state) => state.busyOperation);
  const [seconds, setSeconds] = useState(0);
  // Most imports finish in well under a second (a bracket, an STL). Wait
  // before covering the window so a fast one never flashes a veil.
  const [visible, setVisible] = useState(false);

  useEffect(() => {
    if (!busy) {
      setVisible(false);
      return;
    }
    const timer = window.setTimeout(() => setVisible(true), 350);
    return () => window.clearTimeout(timer);
  }, [busy]);

  useEffect(() => {
    if (!busy) {
      return;
    }
    const tick = () => setSeconds(elapsedSeconds(busy.startedAt, Date.now()));
    tick();
    const timer = window.setInterval(tick, 250);
    return () => window.clearInterval(timer);
  }, [busy]);

  if (!busy || !visible) {
    return null;
  }

  return (
    <div className="cad-modal-backdrop" role="status" aria-live="polite">
      <div
        className="cad-floating-panel flex w-[22rem] max-w-full flex-col gap-3 px-5 py-5"
        data-testid="operation-overlay"
      >
        <div className="flex items-start gap-3">
          <span
            aria-hidden="true"
            className="mt-0.5 h-4 w-4 shrink-0 animate-spin rounded-full border-2 border-on-surface-dim border-t-on-surface"
          />
          <div className="min-w-0 flex-1">
            <p className="text-sm font-medium text-on-surface">
              {t(`busy.${busy.kind}`)}
            </p>
            {busy.fileName ? (
              <p
                className="truncate text-xs text-on-surface-muted"
                title={busy.fileName}
              >
                {busy.fileName}
              </p>
            ) : null}
          </div>
        </div>
        <p className="min-h-4 break-words text-xs leading-5 text-on-surface-muted">
          {busy.detail || t("busy.starting")}
        </p>
        <div className="flex items-center justify-between gap-3">
          <p className="text-[11px] text-on-surface-dim">
            {t("busy.elapsed", { seconds })}
          </p>
          <p className="text-[11px] text-on-surface-dim">{t("busy.patience")}</p>
        </div>
      </div>
    </div>
  );
}
