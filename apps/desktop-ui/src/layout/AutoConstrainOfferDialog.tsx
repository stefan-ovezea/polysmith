import { useEffect } from "react";
import { useTranslation } from "react-i18next";

/**
 * Asked once, on the way out of a sketch that is still far from determined
 * (S11). The prompt exists because "Fully constrain" is invisible otherwise —
 * you have to know the button exists to look for it.
 *
 * It is a question, never an action: the sketch is only constrained if the user
 * says so, and "Don't ask again" silences this sketch for good.
 */
export function AutoConstrainOfferDialog({
  dofs,
  onFullyConstrain,
  onFinishAnyway,
  onDontAskAgain,
  onCancel,
}: {
  dofs: number;
  onFullyConstrain: () => void;
  onFinishAnyway: () => void;
  onDontAskAgain: () => void;
  onCancel: () => void;
}) {
  const { t } = useTranslation();

  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        onCancel();
      }
    }
    window.addEventListener("keydown", handleKeyDown, true);
    return () => {
      window.removeEventListener("keydown", handleKeyDown, true);
    };
  }, [onCancel]);

  return (
    <div
      className="cad-modal-backdrop"
      role="presentation"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) {
          onCancel();
        }
      }}
    >
      <section
        className="cad-generation-result-dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="cad-auto-constrain-title"
      >
        <h2
          id="cad-auto-constrain-title"
          className="text-base font-semibold text-on-surface"
        >
          {t("sketch.autoConstrainOffer.title")}
        </h2>
        <p className="mt-2 text-sm text-on-surface">
          {t("sketch.autoConstrainOffer.body", { count: dofs })}
        </p>
        <p className="mt-2 text-xs text-on-surface-muted">
          {t("sketch.autoConstrainOffer.detail")}
        </p>
        <div className="mt-5 flex flex-col gap-2">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary"
            onClick={onFullyConstrain}
          >
            {t("viewport.fullyConstrain")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action"
            onClick={onFinishAnyway}
          >
            {t("sketch.autoConstrainOffer.finishAnyway")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action"
            onClick={onDontAskAgain}
          >
            {t("sketch.autoConstrainOffer.dontAskAgain")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action"
            onClick={onCancel}
          >
            {t("sketch.autoConstrainOffer.cancel")}
          </button>
        </div>
      </section>
    </div>
  );
}
