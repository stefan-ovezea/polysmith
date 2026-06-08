import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { useConfirmCancelHotkeys } from "./hooks/useConfirmCancelHotkeys";

interface LoftProfileItem {
  profileId: string;
  label: string;
}

interface LoftPreviewPanelProps {
  initialRuled: boolean;
  profiles: LoftProfileItem[];
  disabled: boolean;
  canConfirm: boolean;
  onPreviewRuled: (ruled: boolean) => Promise<void>;
  onMoveProfile: (profileId: string, direction: -1 | 1) => void;
  onRemoveProfile: (profileId: string) => Promise<void>;
  onConfirm: () => void | Promise<void>;
  onCancel: () => Promise<void>;
}

function ChevronUpIcon() {
  return (
    <svg
      aria-hidden="true"
      viewBox="0 0 20 20"
      className="h-4 w-4"
      fill="none"
    >
      <path
        d="M5 12.5 10 7.5l5 5"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function ChevronDownIcon() {
  return (
    <svg
      aria-hidden="true"
      viewBox="0 0 20 20"
      className="h-4 w-4"
      fill="none"
    >
      <path
        d="m5 7.5 5 5 5-5"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function RemoveIcon() {
  return (
    <svg
      aria-hidden="true"
      viewBox="0 0 20 20"
      className="h-4 w-4"
      fill="none"
    >
      <path
        d="m6 6 8 8M14 6l-8 8"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
      />
    </svg>
  );
}

export function LoftPreviewPanel({
  initialRuled,
  profiles,
  disabled,
  canConfirm,
  onPreviewRuled,
  onMoveProfile,
  onRemoveProfile,
  onConfirm,
  onCancel,
}: LoftPreviewPanelProps) {
  const { t } = useTranslation();
  const [ruled, setRuled] = useState(initialRuled);

  useEffect(() => {
    setRuled(initialRuled);
  }, [initialRuled]);

  useConfirmCancelHotkeys({ canConfirm, disabled, onCancel, onConfirm });

  async function setMode(nextRuled: boolean) {
    setRuled(nextRuled);
    await onPreviewRuled(nextRuled);
  }

  return (
    <section className="pointer-events-auto cad-floating-panel box-border w-80 max-w-[calc(100vw-2rem)] overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("panels.loft.title")}</p>
      <div className="mt-3 py-1 text-xs uppercase tracking-[0.16em] text-on-surface-muted">
        {profiles.length >= 2
          ? t("panels.loft.sectionsSelected", { count: profiles.length })
          : t("panels.loft.pickProfiles")}
      </div>
      <div className="mt-4 space-y-4">
        <div className="space-y-2">
          <p className="text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.loft.profileOrder")}
          </p>
          <div className="cad-scrollbar max-h-48 space-y-2 overflow-y-auto overflow-x-hidden pr-1">
            {profiles.length === 0 ? (
              <div className="rounded-md border border-outline/50 px-3 py-3 text-sm text-on-surface-muted">
                {t("panels.loft.emptyProfiles")}
              </div>
            ) : (
              profiles.map((profile, index) => (
                <div
                  key={profile.profileId}
                  className="grid min-w-0 grid-cols-[minmax(0,1fr)_auto] items-center gap-2 rounded-md border border-outline/50 bg-surface-container-low px-2 py-2"
                >
                  <span className="min-w-0 flex-1 truncate text-sm text-on-surface">
                    {profile.label}
                  </span>
                  <div className="grid shrink-0 grid-cols-3 gap-1 text-on-surface-muted">
                    <button
                      type="button"
                      className="cad-icon-button cad-icon-tool h-7 w-7 p-0"
                      aria-label={t("panels.loft.moveProfileUp")}
                      data-tooltip={t("panels.loft.moveProfileUp")}
                      disabled={disabled || index === 0}
                      onClick={() => onMoveProfile(profile.profileId, -1)}
                    >
                      <ChevronUpIcon />
                    </button>
                    <button
                      type="button"
                      className="cad-icon-button cad-icon-tool h-7 w-7 p-0"
                      aria-label={t("panels.loft.moveProfileDown")}
                      data-tooltip={t("panels.loft.moveProfileDown")}
                      disabled={disabled || index === profiles.length - 1}
                      onClick={() => onMoveProfile(profile.profileId, 1)}
                    >
                      <ChevronDownIcon />
                    </button>
                    <button
                      type="button"
                      className="cad-icon-button cad-icon-tool h-7 w-7 p-0 text-danger"
                      aria-label={t("panels.loft.removeProfile")}
                      data-tooltip={t("panels.loft.removeProfile")}
                      disabled={disabled}
                      onClick={() => {
                        void onRemoveProfile(profile.profileId);
                      }}
                    >
                      <RemoveIcon />
                    </button>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
        <fieldset className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          <legend className="mb-2">{t("panels.loft.transition")}</legend>
          <div className="grid min-w-0 grid-cols-2 gap-2">
            <button
              type="button"
              className={`cad-ribbon-action min-w-0 justify-center ${
                !ruled ? "cad-ribbon-action-active" : ""
              }`}
              disabled={disabled}
              onClick={() => {
                void setMode(false);
              }}
            >
              {t("panels.loft.smooth")}
            </button>
            <button
              type="button"
              className={`cad-ribbon-action min-w-0 justify-center ${
                ruled ? "cad-ribbon-action-active" : ""
              }`}
              disabled={disabled}
              onClick={() => {
                void setMode(true);
              }}
            >
              {t("panels.loft.ruled")}
            </button>
          </div>
        </fieldset>
        <div className="flex items-center justify-end gap-2 pt-1">
          <button
            type="button"
            className="cad-ribbon-action"
            disabled={disabled}
            onClick={() => {
              void onCancel();
            }}
          >
            {t("common.cancel")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary"
            disabled={disabled || !canConfirm}
            onClick={() => {
              void onConfirm();
            }}
          >
            {t("common.confirm")}
          </button>
        </div>
      </div>
    </section>
  );
}
