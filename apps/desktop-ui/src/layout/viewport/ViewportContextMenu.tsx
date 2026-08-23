import type { ViewportContextMenuState } from "@/types";

type Translate = (key: string, options?: Record<string, unknown>) => string;

interface ViewportContextMenuProps {
  contextMenu: ViewportContextMenuState;
  translate: Translate;
  getCircleDimensionToggleLabel: (dimensionId: string) => string | null;
  isLinkedBodyCopy: (bodyId: string | null | undefined) => boolean;
  onToggleDimensionDisplay: () => void | Promise<void>;
  onToggleDriven: () => void | Promise<void>;
  onToggleConstruction: () => void | Promise<void>;
  onDeleteDimension: () => void | Promise<void>;
  onDeleteConstraint: () => void | Promise<void>;
  onDeleteSketchSelection: () => void | Promise<void>;
  onMoveCopy: () => void | Promise<void>;
  onTransformArray: () => void | Promise<void>;
  onMoveBody: () => void | Promise<void>;
  onCopyBody: (copyMode: "linked" | "standalone") => void | Promise<void>;
  onUnlinkBodyCopy: () => void | Promise<void>;
  onExportBodyMesh: () => void | Promise<void>;
  onCreateSketch: () => void | Promise<void>;
}

export function ViewportContextMenu({
  contextMenu,
  translate,
  getCircleDimensionToggleLabel,
  isLinkedBodyCopy,
  onToggleDimensionDisplay,
  onToggleDriven,
  onToggleConstruction,
  onDeleteDimension,
  onDeleteConstraint,
  onDeleteSketchSelection,
  onMoveCopy,
  onTransformArray,
  onMoveBody,
  onCopyBody,
  onUnlinkBodyCopy,
  onExportBodyMesh,
  onCreateSketch,
}: ViewportContextMenuProps) {
  const dimensionToggleLabel = contextMenu.dimensionId
    ? getCircleDimensionToggleLabel(contextMenu.dimensionId)
    : null;

  return (
    <div
      className="cad-context-menu absolute z-20 min-w-[160px] rounded-2xl p-1.5 backdrop-blur-xl"
      style={{
        left: contextMenu.x,
        top: contextMenu.y,
        transform: "translate(8px, 8px)",
      }}
    >
      {contextMenu.dimensionId ? (
        <>
          {dimensionToggleLabel ? (
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={onToggleDimensionDisplay}
            >
              {dimensionToggleLabel}
            </button>
          ) : null}
          <button
            type="button"
            className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
            onClick={onToggleDriven}
          >
            Toggle Driving
          </button>
          <button
            type="button"
            className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
            onClick={onDeleteDimension}
          >
            Delete
          </button>
        </>
      ) : contextMenu.constraintKind ? (
        <>
          <button
            type="button"
            className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
            onClick={onDeleteConstraint}
          >
            Delete Constraint
          </button>
          {contextMenu.sketchDeleteSelection ? (
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={onDeleteSketchSelection}
            >
              Delete
            </button>
          ) : null}
        </>
      ) : contextMenu.sketchDeleteSelection ? (
        <>
          {contextMenu.sketchDeleteSelection.entityIds.length > 0 ||
          contextMenu.sketchDeleteSelection.vertexIds.length > 0 ? (
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={onMoveCopy}
            >
              {translate("common.moveCopy")}
            </button>
          ) : null}
          {contextMenu.sketchDeleteSelection.entityIds.length > 0 ? (
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={onTransformArray}
            >
              {translate("common.transformArray")}
            </button>
          ) : null}
          {contextMenu.lineId ? (
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={onToggleConstruction}
            >
              Toggle Construction
            </button>
          ) : null}
          <button
            type="button"
            className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
            onClick={onDeleteSketchSelection}
          >
            Delete
          </button>
        </>
      ) : (
        <>
          {contextMenu.bodyId ? (
            <>
              <button
                type="button"
                className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                onClick={onMoveBody}
              >
                {translate("common.move")}
              </button>
              <div className="group/copy relative">
                <button
                  type="button"
                  className="cad-context-menu-item flex w-full items-center justify-between gap-3 rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                >
                  <span>{translate("common.copy")}</span>
                  <span className="text-on-surface-dim">&gt;</span>
                </button>
                <div className="cad-context-menu invisible absolute left-full top-0 z-30 ml-1 min-w-[180px] rounded-2xl p-1.5 opacity-0 backdrop-blur-xl transition-opacity group-hover/copy:visible group-hover/copy:opacity-100">
                  <button
                    type="button"
                    className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                    onClick={() => {
                      void onCopyBody("linked");
                    }}
                  >
                    {translate("common.copyLinked")}
                  </button>
                  <button
                    type="button"
                    className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                    onClick={() => {
                      void onCopyBody("standalone");
                    }}
                  >
                    {translate("common.copyIndependent")}
                  </button>
                </div>
              </div>
              {isLinkedBodyCopy(contextMenu.bodyId) ? (
                <button
                  type="button"
                  className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                  onClick={onUnlinkBodyCopy}
                >
                  {translate("common.unlink")}
                </button>
              ) : null}
              <button
                type="button"
                className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                onClick={onExportBodyMesh}
              >
                {translate("common.exportAsMesh")}
              </button>
            </>
          ) : null}
          {contextMenu.referenceId || contextMenu.faceId ? (
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={onCreateSketch}
            >
              Create Sketch
            </button>
          ) : null}
        </>
      )}
    </div>
  );
}
