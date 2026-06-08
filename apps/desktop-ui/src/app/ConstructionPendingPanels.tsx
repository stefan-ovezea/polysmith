import { useTranslation } from "react-i18next";
import type { MidplaneAction, PendingReferenceAction } from "./appState";

interface ConstructionPendingPanelsProps {
  constructionAxisAction: PendingReferenceAction | null;
  constructionPointAction: PendingReferenceAction | null;
  disabled: boolean;
  midplaneAction: MidplaneAction | null;
  tangentPlaneAction: PendingReferenceAction | null;
  onCancel: () => void;
}

export function ConstructionPendingPanels({
  constructionAxisAction,
  constructionPointAction,
  disabled,
  midplaneAction,
  tangentPlaneAction,
  onCancel,
}: ConstructionPendingPanelsProps) {
  return (
    <>
      {midplaneAction ? (
        <MidplanePendingPanel
          action={midplaneAction}
          disabled={disabled}
          onCancel={onCancel}
        />
      ) : null}
      {tangentPlaneAction ? (
        <PendingReferencePanel
          disabled={disabled}
          messageKey="panels.tangentPlane.pickSource"
          titleKey="panels.tangentPlane.title"
          onCancel={onCancel}
        />
      ) : null}
      {constructionAxisAction ? (
        <PendingReferencePanel
          disabled={disabled}
          messageKey="panels.constructionAxis.pickSource"
          titleKey="panels.constructionAxis.title"
          onCancel={onCancel}
        />
      ) : null}
      {constructionPointAction ? (
        <PendingReferencePanel
          disabled={disabled}
          messageKey="panels.constructionPoint.pickSource"
          titleKey="panels.constructionPoint.title"
          onCancel={onCancel}
        />
      ) : null}
    </>
  );
}

function MidplanePendingPanel({
  action,
  disabled,
  onCancel,
}: {
  action: MidplaneAction;
  disabled: boolean;
  onCancel: () => void;
}) {
  const { t } = useTranslation();

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">{t("panels.midplane.title")}</p>
      <p className="mt-3 text-xs text-on-surface-muted">
        {action.sourceIds.length === 0
          ? t("panels.midplane.pickFirst")
          : t("panels.midplane.pickSecond")}
      </p>
      <p className="mt-3 text-xs uppercase tracking-[0.16em] text-on-surface-dim">
        {t("panels.midplane.selected", {
          count: action.sourceIds.length,
        })}
      </p>
      <CancelButton disabled={disabled} onCancel={onCancel} />
    </section>
  );
}

function PendingReferencePanel({
  disabled,
  messageKey,
  titleKey,
  onCancel,
}: {
  disabled: boolean;
  messageKey: string;
  titleKey: string;
  onCancel: () => void;
}) {
  const { t } = useTranslation();

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">{t(titleKey)}</p>
      <p className="mt-3 text-xs text-on-surface-muted">{t(messageKey)}</p>
      <CancelButton disabled={disabled} onCancel={onCancel} />
    </section>
  );
}

function CancelButton({
  disabled,
  onCancel,
}: {
  disabled: boolean;
  onCancel: () => void;
}) {
  const { t } = useTranslation();

  return (
    <button
      type="button"
      className="cad-action-ghost mt-4 w-full"
      disabled={disabled}
      onClick={onCancel}
    >
      {t("common.cancel")}
    </button>
  );
}
