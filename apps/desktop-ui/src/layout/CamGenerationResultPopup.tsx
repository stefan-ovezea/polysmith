import { useEffect } from "react";
import { useTranslation } from "react-i18next";
import { useCadCoreStore } from "@/state/cadCoreStore";

// Modal shown after cam_operation_generate completes: success, warnings
// (the generator's warning list — same text the Logs panel shows), or a
// failure message.  Driven by the cam_generation_result core event
// (generate only, never preview), stored in cadCoreStore.generationResult.
export function CamGenerationResultPopup() {
  const { t } = useTranslation();
  const result = useCadCoreStore((state) => state.generationResult);
  const document = useCadCoreStore((state) => state.document);
  const dismiss = useCadCoreStore((state) => state.dismissGenerationResult);

  useEffect(() => {
    if (!result) {
      return;
    }
    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        dismiss();
      }
    }
    window.addEventListener("keydown", handleKeyDown, true);
    return () => {
      window.removeEventListener("keydown", handleKeyDown, true);
    };
  }, [result, dismiss]);

  if (!result) {
    return null;
  }

  // The event follows the document_state reply, so the op's fresh
  // toolpath_cache is already in the store.
  const stats = document?.cam.operations.find(
    (op) => op.op_id === result.op_id,
  )?.toolpath_cache;
  const statsLine =
    stats &&
    (stats.total_length_mm !== undefined ||
      stats.estimated_time_seconds !== undefined)
      ? t("cam.toolpathGenerated", {
          length: stats.total_length_mm?.toFixed(1) ?? "-",
          time: stats.estimated_time_seconds?.toFixed(1) ?? "-",
        })
      : null;

  const title = !result.ok
    ? t("dialogs.generateResult.titleError")
    : result.warnings.length > 0
      ? t("dialogs.generateResult.titleWarnings", {
          count: result.warnings.length,
        })
      : t("dialogs.generateResult.titleSuccess");

  return (
    <div
      className="cad-modal-backdrop"
      role="presentation"
      onMouseDown={(event) => {
        // Backdrop click closes; clicks inside the dialog do not.
        if (event.target === event.currentTarget) {
          dismiss();
        }
      }}
    >
      <section
        className="cad-generation-result-dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="cad-generation-result-title"
      >
        <h2
          id="cad-generation-result-title"
          className={`text-base font-semibold ${
            result.ok
              ? result.warnings.length > 0
                ? "text-on-surface"
                : "text-success"
              : "text-danger"
          }`}
        >
          {title}
        </h2>
        {!result.ok ? (
          <p className="mt-2 text-sm text-danger">{result.error_message}</p>
        ) : (
          <>
            {result.warnings.length > 0 && (
              <ul className="cad-scrollbar mt-2 max-h-[min(50vh,20rem)] list-disc overflow-auto pl-5">
                {result.warnings.map((warning, index) => (
                  <li
                    key={index}
                    className="select-text py-0.5 text-sm text-amber-300"
                  >
                    {warning}
                  </li>
                ))}
              </ul>
            )}
            {statsLine && (
              <p className="mt-2 text-xs text-on-surface-muted">
                {statsLine}
              </p>
            )}
          </>
        )}
        <div className="mt-5 flex justify-end">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary"
            onClick={dismiss}
          >
            {t("dialogs.generateResult.okButton")}
          </button>
        </div>
      </section>
    </div>
  );
}
