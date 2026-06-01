import { useTranslation } from "react-i18next";
import type { CamOperationType } from "./header/CamToolbar";

export interface CamOperation {
  id: string;
  type: CamOperationType;
  name: string;
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

  const typeLabel = (type: CamOperationType): string => {
    switch (type) {
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
                <button
                  type="button"
                  className={
                    selectedOperationId === op.id
                      ? "w-full text-left px-2 py-1.5 rounded text-xs cad-panel-item-active"
                      : "w-full text-left px-2 py-1.5 rounded text-xs cad-panel-item"
                  }
                  onClick={() => onSelectOperation(op.id)}
                >
                  <div className="flex items-center justify-between">
                    <span className="truncate">{op.name}</span>
                    <span className="text-[10px] cad-muted ml-2 shrink-0">
                      {typeLabel(op.type)}
                    </span>
                  </div>
                </button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}
