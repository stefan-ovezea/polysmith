import { useTranslation } from "react-i18next";
import type { CamOperationType } from "./header/CamToolbar";

export interface CamOperation {
  id: string;
  type: CamOperationType;
  name: string;
  mode?: string;
  status?: string;
  statusMessage?: string;
}

export interface CamOperationPanelProps {
  operations: CamOperation[];
  selectedOperationId: string | null;
  onSelectOperation: (id: string) => void;
  onDeleteOperation: (id: string) => void;
}

export function CamOperationPanel({
  operations,
  selectedOperationId,
  onSelectOperation,
  onDeleteOperation,
}: CamOperationPanelProps) {
  const { t } = useTranslation();

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

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between px-3 pt-2 pb-1">
        <span className="text-xs font-semibold cad-muted tracking-wider uppercase">
          {t("cam.operations")}
        </span>
      </div>
      <div className="flex-1 overflow-y-auto px-1">
        {operations.length === 0 ? (
          <p className="px-2 py-4 text-xs cad-muted text-center">
            {t("cam.noOperations")}
          </p>
        ) : (
          <ul className="flex flex-col gap-0.5">
            {operations.map((op) => (
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
                    className="shrink-0 px-1.5 py-1 text-on-surface-muted hover:text-red-400 opacity-0 group-hover:opacity-100 transition-opacity"
                    aria-label={t("cam.faceMilling.delete", "Delete")}
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
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}
