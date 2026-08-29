import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { CamOperationType } from "./header/CamToolbar";

export interface CamOperation {
  id: string;
  // Owning setup ("" for legacy operations — the core joins them to
  // the first setup).
  setupId: string;
  type: CamOperationType;
  name: string;
  mode?: string;
  status?: string;
  statusMessage?: string;
}

export interface CamSetupSummary {
  setup_id: string;
  name: string;
  machine_type: string;
}

export interface CamOperationPanelProps {
  operations: CamOperation[];
  setups: CamSetupSummary[];
  activeSetupId: string | null;
  onSelectSetup: (setupId: string) => void;
  onEditSetup: (setupId: string) => void;
  onDeleteSetup: (setupId: string) => void;
  onNewSetup: () => void;
  selectedOperationId: string | null;
  onSelectOperation: (id: string) => void;
  onDeleteOperation: (id: string) => void;
}

// CAM sidebar tree: setups stacked vertically, each with its own
// operations indented underneath.  The ACTIVE setup receives new
// operations; every setup row edits its own setup.
export function CamOperationPanel({
  operations,
  setups,
  activeSetupId,
  onSelectSetup,
  onEditSetup,
  onDeleteSetup,
  onNewSetup,
  selectedOperationId,
  onSelectOperation,
  onDeleteOperation,
}: CamOperationPanelProps) {
  const { t } = useTranslation();
  // Collapsed setup ids — new setups start expanded.
  const [collapsedIds, setCollapsedIds] = useState<ReadonlySet<string>>(
    new Set(),
  );

  const typeLabel = (operation: CamOperation): string => {
    switch (operation.type) {
      case "faceMilling":
        return t("cam.common.faceOp");
      case "laserCut":
        return operation.mode === "engrave"
          ? t("cam.laserCut.modeEngrave")
          : operation.mode === "score"
            ? t("cam.laserCut.modeScore")
            : t("cam.cutting.twoD");
      case "profile":
        return t("cam.profile");
      case "pocket":
        return t("cam.pocket");
      case "drill":
        return t("cam.drill");
    }
  };

  const machineLabel = (machineType: string): string => {
    switch (machineType) {
      case "laser":
        return t("cam.setupSummary.machineLaser");
      case "plasma":
        return t("cam.setupSummary.machinePlasma");
      case "printer":
        return t("cam.setupSummary.machinePrinter");
      default:
        return machineType;
    }
  };

  // Legacy operations carry an empty setup_id — the core joins them to
  // the first setup, and the tree shows them there.
  const operationsOf = (setupId: string): CamOperation[] =>
    operations.filter(
      (op) =>
        op.setupId === setupId ||
        (op.setupId === "" && setups[0]?.setup_id === setupId),
    );

  return (
    <div className="flex h-full min-h-0 flex-col">
      <div className="flex items-center justify-between px-3 pt-2 pb-1">
        <span className="text-xs font-semibold cad-muted tracking-wider uppercase">
          {t("cam.setupsTitle")}
        </span>
        <button
          type="button"
          className="cad-action-ghost h-6 px-2 text-[10px] uppercase tracking-wider"
          onClick={onNewSetup}
        >
          {t("cam.newSetup")}
        </button>
      </div>

      <div className="min-h-0 flex-1 overflow-y-auto px-1 pb-2">
        {setups.length === 0 ? (
          <p className="px-2 py-4 text-xs cad-muted text-center">
            {t("cam.noSetups")}
          </p>
        ) : (
          <ul className="flex flex-col gap-0.5">
            {setups.map((setup) => {
              const setupOps = operationsOf(setup.setup_id);
              const collapsed = collapsedIds.has(setup.setup_id);
              const isActive = activeSetupId === setup.setup_id;
              return (
                <li key={setup.setup_id}>
                  {/* ── Setup row ─────────────────────────────── */}
                  <div
                    className={
                      isActive
                        ? "flex items-center rounded text-xs cad-panel-item-active"
                        : "flex items-center rounded text-xs cad-panel-item group"
                    }
                  >
                    <button
                      type="button"
                      className="shrink-0 px-1 py-1 text-[9px] cad-muted hover:text-on-surface"
                      aria-label={
                        collapsed
                          ? t("cam.setupRow.expand")
                          : t("cam.setupRow.collapse")
                      }
                      onClick={() => {
                        setCollapsedIds((previous) => {
                          const next = new Set(previous);
                          if (collapsed) {
                            next.delete(setup.setup_id);
                          } else {
                            next.add(setup.setup_id);
                          }
                          return next;
                        });
                      }}
                    >
                      {collapsed ? "▸" : "▾"}
                    </button>
                    <button
                      type="button"
                      className="flex-1 min-w-0 text-left px-1 py-1.5"
                      onClick={() => {
                        // Selecting a collapsed setup reveals its
                        // operations.
                        if (collapsed) {
                          setCollapsedIds((previous) => {
                            const next = new Set(previous);
                            next.delete(setup.setup_id);
                            return next;
                          });
                        }
                        onSelectSetup(setup.setup_id);
                      }}
                    >
                      <div className="flex items-center justify-between gap-2">
                        <span className="truncate">{setup.name}</span>
                        <span className="text-[10px] cad-muted ml-2 shrink-0">
                          {machineLabel(setup.machine_type)}
                        </span>
                      </div>
                    </button>
                    <button
                      type="button"
                      className="shrink-0 px-1.5 py-1 text-[10px] cad-muted underline underline-offset-2 hover:text-on-surface"
                      onClick={() => onEditSetup(setup.setup_id)}
                    >
                      {t("cam.setupSummary.edit")}
                    </button>
                    <button
                      type="button"
                      className="shrink-0 px-1.5 py-1 text-on-surface-muted hover:text-danger"
                      aria-label={t("cam.setupRow.delete")}
                      onClick={(e) => {
                        e.stopPropagation();
                        onDeleteSetup(setup.setup_id);
                      }}
                    >
                      <svg viewBox="0 0 14 14" width="12" height="12" fill="none"
                        stroke="currentColor" strokeWidth="1.5" strokeLinecap="round">
                        <path d="M3 4h8M5 4V3a1 1 0 011-1h2a1 1 0 011 1v1M6 7v3M8 7v3" />
                        <path d="M4 4l.5 7.5a.5.5 0 00.5.5h4a.5.5 0 00.5-.5L10 4" />
                      </svg>
                    </button>
                  </div>

                  {/* ── Operations nested under the setup ─────── */}
                  {!collapsed ? (
                    <ul className="ml-4 flex flex-col gap-0.5 border-l border-[var(--cad-panel-border)] pl-1">
                      {setupOps.length === 0 ? (
                        <li className="px-2 py-1 text-[10px] cad-muted">
                          {t("cam.setupRow.noOperations")}
                        </li>
                      ) : (
                        setupOps.map((op) => (
                          <li key={op.id}>
                            <div
                              className={
                                selectedOperationId === op.id
                                  ? "flex items-center rounded text-xs cad-panel-item-active"
                                  : "flex items-center rounded text-xs cad-panel-item group"
                              }
                            >
                              <button
                                type="button"
                                className="flex-1 text-left px-2 py-1.5"
                                onClick={() => onSelectOperation(op.id)}
                              >
                                <div className="flex items-center justify-between">
                                  <span className="truncate">{op.name}</span>
                                  <span className="text-[10px] cad-muted ml-2 shrink-0">
                                    {typeLabel(op)}
                                  </span>
                                </div>
                              </button>
                              <button
                                type="button"
                                className="shrink-0 px-1.5 py-1 text-on-surface-muted hover:text-danger opacity-0 group-hover:opacity-100 transition-opacity"
                                aria-label={t("cam.operations.delete", "Delete operation")}
                                onClick={(e) => {
                                  e.stopPropagation();
                                  onDeleteOperation(op.id);
                                }}
                              >
                                <svg viewBox="0 0 14 14" width="12" height="12" fill="none"
                                  stroke="currentColor" strokeWidth="1.5" strokeLinecap="round">
                                  <path d="M3 4h8M5 4V3a1 1 0 011-1h2a1 1 0 011 1v1M6 7v3M8 7v3" />
                                  <path d="M4 4l.5 7.5a.5.5 0 00.5.5h4a.5.5 0 00.5-.5L10 4" />
                                </svg>
                              </button>
                            </div>
                          </li>
                        ))
                      )}
                    </ul>
                  ) : null}
                </li>
              );
            })}
          </ul>
        )}
      </div>
    </div>
  );
}
