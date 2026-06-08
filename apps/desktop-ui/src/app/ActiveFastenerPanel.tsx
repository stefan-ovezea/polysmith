import { useTranslation } from "react-i18next";
import { Dropdown, type HoleStandardEntry } from "../lib";
import type { FastenerFeatureParameters } from "../types";
import { ActivePanelActions } from "./ActivePanelActions";
import { HoleStandardSizeDropdown } from "./HoleStandardSizeDropdown";
import {
  fastenerPatchForSize,
  fastenerPatchForStandard,
} from "./threadStandardPatches";

type FastenerStandard = FastenerFeatureParameters["standard"];
type FastenerHeadType = FastenerFeatureParameters["head_type"];
type FastenerDriveType = FastenerFeatureParameters["drive_type"];
type FastenerThreadRepresentation =
  FastenerFeatureParameters["thread_representation"];

interface ActiveFastenerPanelProps {
  disabled: boolean;
  parameters: FastenerFeatureParameters;
  standards: HoleStandardEntry[];
  onCancel: () => void;
  onConfirm: () => void;
  onUpdateParameters: (patch: Partial<FastenerFeatureParameters>) => void;
}

export function ActiveFastenerPanel({
  disabled,
  parameters,
  standards,
  onCancel,
  onConfirm,
  onUpdateParameters,
}: ActiveFastenerPanelProps) {
  const { t } = useTranslation();

  function updateNumber(
    field: "diameter" | "length" | "thread_length",
    value: number,
  ) {
    if (!Number.isFinite(value)) {
      return;
    }
    onUpdateParameters({ [field]: value });
  }

  return (
    <section className="pointer-events-auto cad-floating-panel cad-scrollbar max-h-[min(40rem,calc(100vh-12rem))] w-[21rem] overflow-y-auto px-5 py-5">
      <p className="cad-kicker">{t("panels.fastener.title")}</p>
      <div className="mt-5 space-y-4">
        <div>
          <span className="cad-field-label">
            {t("panels.fastener.standard")}
          </span>
          <Dropdown<FastenerStandard>
            label={t("panels.fastener.standard")}
            className="mt-2"
            value={parameters.standard}
            disabled={disabled}
            options={[
              { value: "custom", label: t("panels.fastener.custom") },
              { value: "metric", label: t("panels.fastener.metric") },
              {
                value: "imperial",
                label: t("panels.fastener.imperial"),
              },
            ]}
            onChange={(value) => {
              const patch = fastenerPatchForStandard(value);
              if (!patch) {
                return;
              }
              onUpdateParameters(patch);
            }}
          />
        </div>
        {parameters.standard !== "custom" ? (
          <div>
            <span className="cad-field-label">{t("panels.fastener.size")}</span>
            <HoleStandardSizeDropdown
              label={t("panels.fastener.size")}
              className="mt-2"
              size={parameters.size}
              standard={parameters.standard}
              disabled={disabled || standards.length === 0}
              standards={standards}
              onChange={(value) => {
                const patch = fastenerPatchForSize(parameters.standard, value);
                if (!patch) {
                  return;
                }
                onUpdateParameters(patch);
              }}
            />
          </div>
        ) : null}
        <div className="grid grid-cols-2 gap-3">
          <label className="block">
            <span className="cad-field-label">
              {t("panels.fastener.diameter")}
            </span>
            <input
              type="number"
              min={0}
              step={0.1}
              className="cad-input mt-2 w-full"
              value={parameters.diameter}
              disabled={disabled}
              onChange={(event) => {
                updateNumber("diameter", event.currentTarget.valueAsNumber);
              }}
            />
          </label>
          <label className="block">
            <span className="cad-field-label">
              {t("panels.fastener.length")}
            </span>
            <input
              type="number"
              min={0}
              step={0.1}
              className="cad-input mt-2 w-full"
              value={parameters.length}
              disabled={disabled}
              onChange={(event) => {
                updateNumber("length", event.currentTarget.valueAsNumber);
              }}
            />
          </label>
        </div>
        <label className="block">
          <span className="cad-field-label">
            {t("panels.fastener.threadLength")}
          </span>
          <input
            type="number"
            min={0}
            step={0.1}
            className="cad-input mt-2 w-full"
            value={parameters.thread_length}
            disabled={disabled}
            onChange={(event) => {
              updateNumber("thread_length", event.currentTarget.valueAsNumber);
            }}
          />
        </label>
        <div>
          <span className="cad-field-label">
            {t("panels.fastener.headType")}
          </span>
          <Dropdown<FastenerHeadType>
            label={t("panels.fastener.headType")}
            className="mt-2"
            value={parameters.head_type}
            disabled={disabled}
            options={[
              {
                value: "socket_head",
                label: t("panels.fastener.socketHead"),
              },
              {
                value: "button_head",
                label: t("panels.fastener.buttonHead"),
              },
              { value: "flat", label: t("panels.fastener.flat") },
              {
                value: "hex_bolt",
                label: t("panels.fastener.hexBolt"),
              },
            ]}
            onChange={(value) => {
              onUpdateParameters({ head_type: value });
            }}
          />
        </div>
        <div>
          <span className="cad-field-label">
            {t("panels.fastener.driveType")}
          </span>
          <Dropdown<FastenerDriveType>
            label={t("panels.fastener.driveType")}
            className="mt-2"
            value={parameters.drive_type}
            disabled={disabled}
            options={[
              { value: "none", label: t("panels.fastener.none") },
              {
                value: "hex_socket",
                label: t("panels.fastener.hexSocket"),
              },
              {
                value: "phillips",
                label: t("panels.fastener.phillips"),
              },
            ]}
            onChange={(value) => {
              onUpdateParameters({ drive_type: value });
            }}
          />
        </div>
        <div className="rounded-md border border-outline/50 bg-surface-container-low px-3 py-2">
          <span className="cad-field-label">
            {t("panels.fastener.threadRepresentation")}
          </span>
          <Dropdown<FastenerThreadRepresentation>
            label={t("panels.fastener.threadRepresentation")}
            className="mt-2"
            value={parameters.thread_representation}
            disabled={disabled}
            options={[
              {
                value: "cosmetic",
                label: t("panels.fastener.cosmetic"),
              },
              {
                value: "modeled",
                label: t("panels.fastener.modeled"),
              },
            ]}
            onChange={(value) => {
              onUpdateParameters({ thread_representation: value });
            }}
          />
          <p className="mt-1 text-[11px] leading-4 text-on-surface-dim">
            {t("panels.fastener.modeledUnavailable")}
          </p>
        </div>
      </div>
      <ActivePanelActions
        disabled={disabled}
        onCancel={onCancel}
        onConfirm={onConfirm}
      />
    </section>
  );
}
