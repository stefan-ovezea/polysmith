import { useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import { useTranslation } from "react-i18next";

import type { FeatureEntry, PcbLayerParameters, PcbRole } from "@/types";

// The `pcb` workspace: import a fabrication set (Gerber + Excellon), see what
// each file turned out to be, and correct the layer table by hand.
//
// It deliberately does NOT post G-code or invent its own toolpaths — its only
// output is `pcb_layer` document features, which the CAM operations consume
// (wiki/PCB-Workspace.md §1).  Creating those operations lands with the
// region-attestation work, so this page is the import half: honest roles,
// counts, and an unmissable list of anything the parser had to skip.

interface PcbWorkspaceProps {
  features: FeatureEntry[];
  onImportLayer: (filePath: string, role?: string) => Promise<void>;
  onUpdateLayer: (
    featureId: string,
    options: { role?: string; mirrorX?: boolean },
  ) => Promise<void>;
  onRemoveLayer: (featureId: string) => Promise<void>;
  addMessage: (message: string) => void;
}

// The roles a user can pick, in board order. Mirrors `pcb_role_id` in the core.
const ROLE_OPTIONS: PcbRole[] = [
  "copper_top",
  "copper_inner",
  "copper_bottom",
  "solder_mask_top",
  "solder_mask_bottom",
  "paste_top",
  "paste_bottom",
  "silkscreen_top",
  "silkscreen_bottom",
  "profile",
  "drill_plated",
  "drill_non_plated",
];

function layerOf(feature: FeatureEntry): PcbLayerParameters | null {
  return feature.pcb_layer_parameters ?? null;
}

function fileName(path: string): string {
  const parts = path.split(/[\\/]/);
  return parts[parts.length - 1] || path;
}

// A 4-layer board has two "Inner copper" rows, so the table has to say which
// stack-up layer each one is. The ordinal lives in the file's own attribute
// ("Copper,L2,Inr" / "Copper,L3,Inr").
function layerOrdinal(fileFunction: string): string | null {
  const match = /,L(\d+),/i.exec(fileFunction);
  return match ? `L${match[1]}` : null;
}

export function PcbWorkspace({
  features,
  onImportLayer,
  onUpdateLayer,
  onRemoveLayer,
  addMessage,
}: PcbWorkspaceProps) {
  const { t } = useTranslation();
  const [busy, setBusy] = useState(false);

  const layers = features.filter((feature) => layerOf(feature) !== null);

  const importSet = async () => {
    const selection = await open({
      multiple: true,
      filters: [
        {
          name: t("pcb.filterFabrication"),
          extensions: ["gbr", "ger", "gtl", "gbl", "gts", "gbs", "gto", "gbo", "drl", "txt"],
        },
      ],
    });
    if (!selection) {
      return;
    }
    const paths = Array.isArray(selection) ? selection : [selection];
    setBusy(true);
    try {
      for (const path of paths) {
        // Imported one at a time: each reply carries a fresh document state,
        // so the table fills in as the set loads and one bad file cannot stop
        // the rest.
        await onImportLayer(path);
      }
      addMessage(t("pcb.importedCount", { count: paths.length }));
    } catch (error) {
      addMessage(t("pcb.importFailed", { error: String(error) }));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="flex h-full min-h-0 flex-col gap-3 overflow-auto p-4">
      <header className="flex items-center justify-between gap-4">
        <div>
          <p className="cad-kicker">{t("pcb.title")}</p>
          <p className="text-[11px] text-on-surface-dim">{t("pcb.subtitle")}</p>
        </div>
        <button
          type="button"
          className="cad-action-ghost h-8"
          disabled={busy}
          onClick={() => {
            void importSet();
          }}
        >
          {busy ? t("pcb.importing") : t("pcb.importSet")}
        </button>
      </header>

      {layers.length === 0 ? (
        <p className="text-[11px] text-on-surface-dim">{t("pcb.emptyHint")}</p>
      ) : (
        <table className="w-full border-collapse text-[11px]">
          <thead>
            <tr className="text-left text-on-surface-dim">
              <th className="py-1 pr-3 font-medium">{t("pcb.columnLayer")}</th>
              <th className="py-1 pr-3 font-medium">{t("pcb.columnRole")}</th>
              <th className="py-1 pr-3 font-medium">{t("pcb.columnContents")}</th>
              <th className="py-1 pr-3 font-medium">{t("pcb.columnPolarity")}</th>
              <th className="py-1 pr-3 font-medium">{t("pcb.columnMirror")}</th>
              <th className="py-1 font-medium" />
            </tr>
          </thead>
          <tbody>
            {layers.map((feature) => {
              const layer = layerOf(feature);
              if (layer === null) {
                return null;
              }
              const contents =
                layer.format === "excellon"
                  ? t("pcb.contentsDrills", {
                      tools: layer.tool_count,
                      holes: layer.hit_count,
                    })
                  : t("pcb.contentsRings", { count: layer.ring_count });
              return (
                <tr
                  key={feature.feature_id}
                  className="border-t border-[var(--cad-panel-soft-border)] align-top"
                >
                  <td className="py-1.5 pr-3">
                    <div className="font-medium">{fileName(layer.file_path)}</div>
                    {/* The core's parameters_summary is for the timeline, where
                        it carries the file identity next to the role label; here
                        the filename is already the row header, so only the
                        problems are repeated. */}
                    {feature.dependency_broken ? (
                      <div className="text-[var(--cad-danger)]">
                        {feature.dependency_warning || t("pcb.sourceMissing")}
                      </div>
                    ) : null}
                    {layer.warnings.length > 0 ? (
                      <details className="text-on-surface-dim">
                        <summary>
                          {t("pcb.warningCount", { count: layer.warnings.length })}
                        </summary>
                        <ul className="mt-1 list-disc pl-4">
                          {layer.warnings.map((warning, index) => (
                            <li key={`${feature.feature_id}-w${index}`}>{warning}</li>
                          ))}
                        </ul>
                      </details>
                    ) : null}
                  </td>
                  <td className="py-1.5 pr-3">
                    <select
                      className="cad-input h-7"
                      value={layer.role}
                      onChange={(event) => {
                        void onUpdateLayer(feature.feature_id, {
                          role: event.target.value,
                        });
                      }}
                    >
                      {ROLE_OPTIONS.map((role) => {
                        const ordinal =
                          role === "copper_inner" ? layerOrdinal(layer.file_function) : null;
                        return (
                          <option key={role} value={role}>
                            {ordinal
                              ? t("pcb.roleWithOrdinal", {
                                  role: t(`pcb.role.${role}`),
                                  ordinal,
                                })
                              : t(`pcb.role.${role}`)}
                          </option>
                        );
                      })}
                    </select>
                    <div className="text-on-surface-dim">
                      {layer.role_from_attributes
                        ? t("pcb.roleFromFile", { function: layer.file_function })
                        : t("pcb.roleGuessed")}
                    </div>
                  </td>
                  <td className="py-1.5 pr-3">
                    <div>{contents}</div>
                    {layer.bounds_valid ? (
                      <div className="text-on-surface-dim">
                        {t("pcb.bounds", {
                          width: (layer.max_x - layer.min_x).toFixed(1),
                          height: (layer.max_y - layer.min_y).toFixed(1),
                        })}
                      </div>
                    ) : null}
                  </td>
                  <td className="py-1.5 pr-3">{layer.file_polarity}</td>
                  <td className="py-1.5 pr-3">
                    <label className="flex items-center gap-1.5">
                      <input
                        type="checkbox"
                        checked={layer.mirror_x}
                        aria-label={t("pcb.columnMirror")}
                        onChange={(event) => {
                          void onUpdateLayer(feature.feature_id, {
                            mirrorX: event.target.checked,
                          });
                        }}
                      />
                      <span className="text-on-surface-dim">
                        {layer.mirror_x ? t("pcb.mirrored") : t("pcb.notMirrored")}
                      </span>
                    </label>
                  </td>
                  <td className="py-1.5">
                    <button
                      type="button"
                      className="cad-action-ghost h-7"
                      onClick={() => {
                        void onRemoveLayer(feature.feature_id);
                      }}
                    >
                      {t("pcb.remove")}
                    </button>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      )}

      <p className="text-[11px] text-on-surface-dim">{t("pcb.operationsHint")}</p>
    </div>
  );
}
