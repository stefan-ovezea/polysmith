import { useTranslation } from "react-i18next";
import {
  Dropdown,
  findHoleStandard,
  holeStandardsForMode,
  type HoleStandardEntry,
} from "../lib";
import type { ThreadFeatureParameters } from "../types";
import type { ThreadAction } from "./appState";

type ThreadMode = ThreadFeatureParameters["mode"];
type ThreadStandard = ThreadFeatureParameters["standard"];
type ThreadHandedness = ThreadFeatureParameters["handedness"];
type ThreadRepresentation = ThreadFeatureParameters["representation"];

interface ActiveThreadPanelProps {
  action: ThreadAction;
  axisLabel: string;
  disabled: boolean;
  parameters: ThreadFeatureParameters | null;
  pendingAxisLabel: string | null;
  standards: HoleStandardEntry[];
  targetLabel: string;
  onCancel: () => void;
  onConfirm: () => void;
  onUpdateParameters: (patch: Partial<ThreadFeatureParameters>) => void;
}

export function ActiveThreadPanel({
  action,
  axisLabel,
  disabled,
  parameters,
  pendingAxisLabel,
  standards,
  targetLabel,
  onCancel,
  onConfirm,
  onUpdateParameters,
}: ActiveThreadPanelProps) {
  const { t } = useTranslation();

  return (
    <section className="pointer-events-auto cad-floating-panel cad-scrollbar max-h-[min(42rem,calc(100vh-12rem))] w-[21rem] overflow-y-auto px-5 py-5">
      <p className="cad-kicker">{t("panels.thread.title")}</p>
      {action.phase !== "active" ? (
        <PendingThreadPrompt
          action={action}
          axisLabel={pendingAxisLabel}
          disabled={disabled}
          onCancel={onCancel}
        />
      ) : parameters ? (
        <>
          <ThreadSourceSummary
            axisLabel={axisLabel}
            targetLabel={targetLabel}
          />
          <div className="mt-5 space-y-4">
            <ThreadParameterFields
              disabled={disabled}
              parameters={parameters}
              standards={standards}
              onUpdateParameters={onUpdateParameters}
            />
          </div>
          <ThreadPanelActions
            disabled={disabled}
            onCancel={onCancel}
            onConfirm={onConfirm}
          />
        </>
      ) : null}
    </section>
  );
}

function PendingThreadPrompt({
  action,
  axisLabel,
  disabled,
  onCancel,
}: {
  action: Exclude<ThreadAction, { phase: "active" }>;
  axisLabel: string | null;
  disabled: boolean;
  onCancel: () => void;
}) {
  const { t } = useTranslation();

  return (
    <>
      {action.phase === "pick_axis" ? (
        <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
          <span>{t("panels.thread.target")}</span>
          <span className="truncate text-right text-on-surface">
            {action.targetSummary}
          </span>
        </div>
      ) : axisLabel ? (
        <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
          <span>{t("panels.thread.axis")}</span>
          <span className="truncate text-right text-on-surface">
            {axisLabel}
          </span>
        </div>
      ) : null}
      <p className="mt-3 text-xs text-on-surface-muted">
        {action.phase === "pick_axis"
          ? t("panels.thread.pickAxis")
          : t("panels.thread.pickTarget")}
      </p>
      <button
        type="button"
        className="cad-action-ghost mt-4 w-full"
        disabled={disabled}
        onClick={onCancel}
      >
        {t("common.cancel")}
      </button>
    </>
  );
}

function ThreadSourceSummary({
  axisLabel,
  targetLabel,
}: {
  axisLabel: string;
  targetLabel: string;
}) {
  const { t } = useTranslation();

  return (
    <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
      <span>{t("panels.thread.target")}</span>
      <span className="truncate text-right text-on-surface">{targetLabel}</span>
      <span>{t("panels.thread.axis")}</span>
      <span className="truncate text-right text-on-surface">{axisLabel}</span>
    </div>
  );
}

function ThreadParameterFields({
  disabled,
  parameters,
  standards,
  onUpdateParameters,
}: {
  disabled: boolean;
  parameters: ThreadFeatureParameters;
  standards: HoleStandardEntry[];
  onUpdateParameters: (patch: Partial<ThreadFeatureParameters>) => void;
}) {
  const { t } = useTranslation();

  function updateNumber(
    field: "major_diameter" | "pitch" | "length" | "start_offset",
    value: number,
  ) {
    if (!Number.isFinite(value)) {
      return;
    }
    onUpdateParameters({ [field]: value });
  }

  return (
    <>
      <div>
        <span className="cad-field-label">{t("panels.thread.mode")}</span>
        <Dropdown<ThreadMode>
          label={t("panels.thread.mode")}
          className="mt-2"
          value={parameters.mode}
          disabled={disabled}
          options={[
            {
              value: "external",
              label: t("panels.thread.external"),
            },
            {
              value: "internal",
              label: t("panels.thread.internal"),
            },
          ]}
          onChange={(value) => {
            onUpdateParameters({ mode: value });
          }}
        />
      </div>
      <div>
        <span className="cad-field-label">{t("panels.thread.standard")}</span>
        <Dropdown<ThreadStandard>
          label={t("panels.thread.standard")}
          className="mt-2"
          value={parameters.standard}
          disabled={disabled}
          options={[
            { value: "custom", label: t("panels.thread.custom") },
            { value: "metric", label: t("panels.thread.metric") },
            {
              value: "imperial",
              label: t("panels.thread.imperial"),
            },
          ]}
          onChange={(value) => {
            if (value === "custom") {
              onUpdateParameters({
                standard: value,
                size: "",
              });
              return;
            }
            const entry = holeStandardsForMode(value)[0];
            if (!entry) {
              return;
            }
            onUpdateParameters({
              standard: value,
              size: entry.id,
              major_diameter: entry.majorDiameter,
              minor_diameter: entry.minorDiameter,
              pitch: entry.pitch,
            });
          }}
        />
      </div>
      {parameters.standard !== "custom" ? (
        <div>
          <span className="cad-field-label">{t("panels.thread.size")}</span>
          <Dropdown<string>
            label={t("panels.thread.size")}
            className="mt-2"
            value={
              findHoleStandard(parameters.standard, parameters.size)?.id ??
              standards[0]?.id ??
              ""
            }
            disabled={disabled || standards.length === 0}
            options={standards.map((entry) => ({
              value: entry.id,
              label: entry.label,
            }))}
            onChange={(value) => {
              const entry = findHoleStandard(parameters.standard, value);
              if (!entry) {
                return;
              }
              onUpdateParameters({
                size: entry.id,
                major_diameter: entry.majorDiameter,
                minor_diameter: entry.minorDiameter,
                pitch: entry.pitch,
              });
            }}
          />
        </div>
      ) : null}
      <div className="grid grid-cols-2 gap-3">
        <ThreadNumberField
          disabled={disabled}
          label={t("panels.thread.majorDiameter")}
          min={0}
          value={parameters.major_diameter}
          onChange={(value) => updateNumber("major_diameter", value)}
        />
        <ThreadNumberField
          disabled={disabled}
          label={t("panels.thread.pitch")}
          min={0}
          value={parameters.pitch}
          onChange={(value) => updateNumber("pitch", value)}
        />
      </div>
      <div className="grid grid-cols-2 gap-3">
        <ThreadNumberField
          disabled={disabled}
          label={t("panels.thread.length")}
          min={0}
          value={parameters.length}
          onChange={(value) => updateNumber("length", value)}
        />
        <ThreadNumberField
          disabled={disabled}
          label={t("panels.thread.startOffset")}
          value={parameters.start_offset}
          onChange={(value) => updateNumber("start_offset", value)}
        />
      </div>
      <div>
        <span className="cad-field-label">{t("panels.thread.handedness")}</span>
        <Dropdown<ThreadHandedness>
          label={t("panels.thread.handedness")}
          className="mt-2"
          value={parameters.handedness}
          disabled={disabled}
          options={[
            {
              value: "right",
              label: t("panels.thread.rightHand"),
            },
            {
              value: "left",
              label: t("panels.thread.leftHand"),
            },
          ]}
          onChange={(value) => {
            onUpdateParameters({ handedness: value });
          }}
        />
      </div>
      <div className="rounded-md border border-outline/50 bg-surface-container-low px-3 py-2">
        <span className="cad-field-label">
          {t("panels.thread.representation")}
        </span>
        <Dropdown<ThreadRepresentation>
          label={t("panels.thread.representation")}
          className="mt-2"
          value={parameters.representation}
          disabled={disabled}
          options={[
            {
              value: "cosmetic",
              label: t("panels.thread.cosmetic"),
            },
            {
              value: "modeled",
              label: t("panels.thread.modeled"),
            },
          ]}
          onChange={(value) => {
            onUpdateParameters({ representation: value });
          }}
        />
        <p className="mt-1 text-[11px] leading-4 text-on-surface-dim">
          {t("panels.thread.modeledUnavailable")}
        </p>
      </div>
    </>
  );
}

function ThreadNumberField({
  disabled,
  label,
  min,
  value,
  onChange,
}: {
  disabled: boolean;
  label: string;
  min?: number;
  value: number;
  onChange: (value: number) => void;
}) {
  return (
    <label className="block">
      <span className="cad-field-label">{label}</span>
      <input
        type="number"
        min={min}
        step={0.1}
        className="cad-input mt-2 w-full"
        value={value}
        disabled={disabled}
        onChange={(event) => {
          onChange(event.currentTarget.valueAsNumber);
        }}
      />
    </label>
  );
}

function ThreadPanelActions({
  disabled,
  onCancel,
  onConfirm,
}: {
  disabled: boolean;
  onCancel: () => void;
  onConfirm: () => void;
}) {
  const { t } = useTranslation();

  return (
    <div className="mt-5 flex gap-3">
      <button
        type="button"
        className="cad-ribbon-action cad-ribbon-action-primary flex-1"
        disabled={disabled}
        onClick={onConfirm}
      >
        {t("common.confirm")}
      </button>
      <button
        type="button"
        className="cad-ribbon-action flex-1"
        onClick={onCancel}
      >
        {t("common.cancel")}
      </button>
    </div>
  );
}
