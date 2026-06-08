import { useTranslation } from "react-i18next";
import {
  Checkbox,
  Dropdown,
  applyHoleStandard,
  findHoleStandard,
  holeStandardsForMode,
  type HoleStandardEntry,
} from "../lib";
import type {
  HoleFeatureParameters,
  HoleFit,
  HoleStandard,
} from "../types";
import type { HoleAction } from "./appState";

type HoleType = HoleFeatureParameters["hole_type"];
type HoleExtentType = HoleFeatureParameters["extent_type"];
type ThreadRepresentation = HoleFeatureParameters["thread_representation"];

interface ActiveHolePanelProps {
  action: HoleAction;
  disabled: boolean;
  parameters: HoleFeatureParameters | null;
  standards: HoleStandardEntry[];
  onCancel: () => void;
  onConfirm: () => void;
  onUpdateParameters: (patch: Partial<HoleFeatureParameters>) => void;
}

export function ActiveHolePanel({
  action,
  disabled,
  parameters,
  standards,
  onCancel,
  onConfirm,
  onUpdateParameters,
}: ActiveHolePanelProps) {
  const { t } = useTranslation();

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">{t("panels.hole.title")}</p>
          <p className="mt-3 text-sm tracking-[0.18em] text-[color:var(--cad-muted)] uppercase">
            {action.phase === "pending"
              ? t("panels.hole.pickFace")
              : t("panels.hole.faceSelected")}
          </p>
        </div>
        {parameters ? (
          <HoleParameterFields
            disabled={disabled}
            parameters={parameters}
            standards={standards}
            onUpdateParameters={onUpdateParameters}
          />
        ) : null}
        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled || action.phase !== "active"}
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
      </div>
    </section>
  );
}

function HoleParameterFields({
  disabled,
  parameters,
  standards,
  onUpdateParameters,
}: {
  disabled: boolean;
  parameters: HoleFeatureParameters;
  standards: HoleStandardEntry[];
  onUpdateParameters: (patch: Partial<HoleFeatureParameters>) => void;
}) {
  const { t } = useTranslation();

  function updatePositiveNumber(
    patchFromValue: (value: number) => Partial<HoleFeatureParameters>,
    value: number,
  ) {
    if (!Number.isFinite(value) || value <= 0) {
      return;
    }
    onUpdateParameters(patchFromValue(value));
  }

  return (
    <>
      <Dropdown<HoleStandard>
        value={parameters.standard}
        label={t("panels.hole.standard")}
        options={[
          {
            value: "custom",
            label: t("panels.hole.custom"),
          },
          {
            value: "metric",
            label: t("panels.hole.metric"),
          },
          {
            value: "imperial",
            label: t("panels.hole.imperial"),
          },
        ]}
        disabled={disabled}
        onChange={(standard) => {
          const nextStandards = holeStandardsForMode(standard);
          if (standard === "custom" || nextStandards.length === 0) {
            onUpdateParameters({
              standard: "custom",
              standard_size: "",
              hole_fit: "clearance",
            });
            return;
          }
          onUpdateParameters(
            applyHoleStandard(
              {
                ...parameters,
                standard,
                standard_size: nextStandards[0].id,
              },
              nextStandards[0],
              parameters.hole_fit,
            ),
          );
        }}
      />
      {parameters.standard !== "custom" ? (
        <div className="grid grid-cols-2 gap-3">
          <Dropdown<string>
            value={parameters.standard_size || standards[0]?.id || ""}
            label={t("panels.hole.size")}
            options={standards.map((entry) => ({
              value: entry.id,
              label: entry.label,
            }))}
            disabled={disabled || standards.length === 0}
            onChange={(standardSize) => {
              const entry = findHoleStandard(parameters.standard, standardSize);
              if (!entry) {
                return;
              }
              onUpdateParameters(
                applyHoleStandard(parameters, entry, parameters.hole_fit),
              );
            }}
          />
          <Dropdown<HoleFit>
            value={parameters.hole_fit}
            label={t("panels.hole.fit")}
            options={[
              {
                value: "clearance",
                label: t("panels.hole.clearance"),
              },
              {
                value: "tap_drill",
                label: t("panels.hole.tapDrill"),
              },
              {
                value: "threaded",
                label: t("panels.hole.threaded"),
              },
            ]}
            disabled={disabled}
            onChange={(fit) => {
              const entry = findHoleStandard(
                parameters.standard,
                parameters.standard_size,
              );
              if (!entry) {
                return;
              }
              onUpdateParameters(applyHoleStandard(parameters, entry, fit));
            }}
          />
        </div>
      ) : null}
      <Dropdown<HoleType>
        value={parameters.hole_type}
        label={t("panels.hole.type")}
        options={[
          {
            value: "simple",
            label: t("panels.hole.simple"),
          },
          {
            value: "counterbore",
            label: t("panels.hole.counterbore"),
          },
          {
            value: "countersink",
            label: t("panels.hole.countersink"),
          },
          {
            value: "spotface",
            label: t("panels.hole.spotface"),
          },
        ]}
        disabled={disabled}
        onChange={(holeType) => {
          onUpdateParameters({ hole_type: holeType });
        }}
      />
      <Dropdown<HoleExtentType>
        value={parameters.extent_type}
        label={t("panels.hole.extent")}
        options={[
          {
            value: "blind",
            label: t("panels.hole.blind"),
          },
          {
            value: "through_all",
            label: t("panels.hole.throughAll"),
          },
        ]}
        disabled={disabled}
        onChange={(extentType) => {
          onUpdateParameters({ extent_type: extentType });
        }}
      />
      <HoleNumberField
        disabled={disabled}
        label={t("panels.hole.diameter")}
        min="0.01"
        value={parameters.diameter}
        onChange={(value) => updatePositiveNumber((diameter) => ({ diameter }), value)}
      />
      {parameters.extent_type === "blind" ? (
        <HoleNumberField
          disabled={disabled}
          label={t("panels.hole.depth")}
          min="0.01"
          value={parameters.depth}
          onChange={(value) =>
            updatePositiveNumber(
              (depth) => ({
                depth,
                thread_depth: parameters.thread_enabled
                  ? Math.min(parameters.thread_depth, depth)
                  : parameters.thread_depth,
              }),
              value,
            )
          }
        />
      ) : null}
      {parameters.hole_type === "counterbore" ||
      parameters.hole_type === "spotface" ? (
        <div className="grid grid-cols-2 gap-3">
          <HoleNumberField
            disabled={disabled}
            label={t("panels.hole.counterboreDiameter")}
            min="0.01"
            value={parameters.counterbore_diameter}
            onChange={(value) =>
              updatePositiveNumber(
                (counterboreDiameter) => ({
                  counterbore_diameter: counterboreDiameter,
                }),
                value,
              )
            }
          />
          <HoleNumberField
            disabled={disabled}
            label={t("panels.hole.counterboreDepth")}
            min="0.01"
            value={parameters.counterbore_depth}
            onChange={(value) =>
              updatePositiveNumber(
                (counterboreDepth) => ({
                  counterbore_depth: counterboreDepth,
                }),
                value,
              )
            }
          />
        </div>
      ) : null}
      {parameters.hole_type === "countersink" ? (
        <div className="grid grid-cols-2 gap-3">
          <HoleNumberField
            disabled={disabled}
            label={t("panels.hole.countersinkDiameter")}
            min="0.01"
            value={parameters.countersink_diameter}
            onChange={(value) =>
              updatePositiveNumber(
                (countersinkDiameter) => ({
                  countersink_diameter: countersinkDiameter,
                }),
                value,
              )
            }
          />
          <HoleNumberField
            disabled={disabled}
            label={t("panels.hole.countersinkAngle")}
            max="179"
            min="1"
            value={parameters.countersink_angle_degrees}
            onChange={(value) =>
              updatePositiveNumber(
                (countersinkAngle) => ({
                  countersink_angle_degrees: countersinkAngle,
                }),
                value,
              )
            }
          />
        </div>
      ) : null}
      <label className="flex items-center justify-between gap-3 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
        <span>{t("panels.hole.threaded")}</span>
        <Checkbox
          checked={parameters.thread_enabled}
          ariaLabel={t("panels.hole.threaded")}
          disabled={disabled}
          onCheckedChange={(checked) => {
            onUpdateParameters({
              thread_enabled: checked,
              thread_representation: checked
                ? parameters.thread_representation
                : "cosmetic",
            });
          }}
        />
      </label>
      {parameters.thread_enabled ? (
        <div>
          <span className="cad-field-label">
            {t("panels.hole.representation")}
          </span>
          <Dropdown<ThreadRepresentation>
            label={t("panels.hole.representation")}
            className="mt-2"
            value={parameters.thread_representation}
            disabled={disabled}
            options={[
              {
                value: "cosmetic",
                label: t("panels.hole.cosmetic"),
              },
              {
                value: "modeled",
                label: t("panels.hole.modeled"),
              },
            ]}
            onChange={(value) => {
              onUpdateParameters({ thread_representation: value });
            }}
          />
          <p className="mt-1 text-xs text-[color:var(--cad-muted)]">
            {t("panels.hole.cosmeticThreadOnly")}
          </p>
        </div>
      ) : null}
    </>
  );
}

function HoleNumberField({
  disabled,
  label,
  max,
  min,
  value,
  onChange,
}: {
  disabled: boolean;
  label: string;
  max?: string;
  min: string;
  value: number;
  onChange: (value: number) => void;
}) {
  return (
    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
      <span>{label}</span>
      <input
        className="cad-input mt-2"
        type="number"
        min={min}
        max={max}
        step="any"
        value={value}
        disabled={disabled}
        onChange={(event) => {
          onChange(Number(event.target.value));
        }}
      />
    </label>
  );
}
