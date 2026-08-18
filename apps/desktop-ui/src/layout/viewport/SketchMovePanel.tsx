import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

// Floating Move/Copy dialog (Fusion-style): numeric dx/dy/angle fields
// plus OK/Cancel.  Drags in the viewport accumulate into the pending
// transform and update these fields; editing a field re-previews the
// pending transform immediately.  OK commits the total transform as a
// single undo step; Cancel reverts the preview.

interface SketchMovePanelProps {
  values: { dx: number; dy: number; angleDeg: number };
  onValuesChange: (values: { dx: number; dy: number; angleDeg: number }) => void;
  onCommit: () => void | Promise<void>;
  onCancel: () => void | Promise<void>;
}

function round3(value: number) {
  return Math.round(value * 1000) / 1000;
}

export function SketchMovePanel({
  values,
  onValuesChange,
  onCommit,
  onCancel,
}: SketchMovePanelProps) {
  const { t } = useTranslation();
  // Local field state so typing isn't clobbered mid-keystroke; synced
  // from the viewport values whenever a drag ends.
  const [dx, setDx] = useState(String(round3(values.dx)));
  const [dy, setDy] = useState(String(round3(values.dy)));
  const [angle, setAngle] = useState(String(round3(values.angleDeg)));
  const valuesRef = useRef(values);
  valuesRef.current = values;
  useEffect(() => {
    setDx(String(round3(valuesRef.current.dx)));
    setDy(String(round3(valuesRef.current.dy)));
    setAngle(String(round3(valuesRef.current.angleDeg)));
  }, [values.dx, values.dy, values.angleDeg]);

  const commitField = (
    text: string,
    current: number,
    apply: (next: number) => void,
  ) => {
    const parsed = Number(text);
    if (!Number.isFinite(parsed) || parsed === current) {
      return;
    }
    apply(parsed);
  };

  const commitAll = () => {
    commitField(dx, valuesRef.current.dx, (next) =>
      onValuesChange({ ...valuesRef.current, dx: next }),
    );
    commitField(dy, valuesRef.current.dy, (next) =>
      onValuesChange({ ...valuesRef.current, dy: next }),
    );
    commitField(angle, valuesRef.current.angleDeg, (next) =>
      onValuesChange({ ...valuesRef.current, angleDeg: next }),
    );
  };

  return (
    <div className="cad-context-menu absolute bottom-4 right-4 z-20 flex min-w-[200px] flex-col gap-2 p-3">
      <div className="text-sm font-medium text-on-surface">
        {t("panels.sketchMove.title")}
      </div>
      <div className="flex items-center gap-2">
        <label className="w-12 text-xs text-on-surface-dim">
          {t("panels.sketchMove.dx")}
        </label>
        <input
          type="number"
          className="h-8 w-full min-w-0 flex-1 rounded-lg border border-outline/50 bg-surface-container-low px-2 text-sm text-on-surface"
          value={dx}
          onChange={(event) => setDx(event.target.value)}
          onBlur={commitAll}
          onKeyDown={(event) => {
            if (event.key === "Enter") {
              commitAll();
            }
          }}
        />
      </div>
      <div className="flex items-center gap-2">
        <label className="w-12 text-xs text-on-surface-dim">
          {t("panels.sketchMove.dy")}
        </label>
        <input
          type="number"
          className="h-8 w-full min-w-0 flex-1 rounded-lg border border-outline/50 bg-surface-container-low px-2 text-sm text-on-surface"
          value={dy}
          onChange={(event) => setDy(event.target.value)}
          onBlur={commitAll}
          onKeyDown={(event) => {
            if (event.key === "Enter") {
              commitAll();
            }
          }}
        />
      </div>
      <div className="flex items-center gap-2">
        <label className="w-12 text-xs text-on-surface-dim">
          {t("panels.sketchMove.angle")}
        </label>
        <input
          type="number"
          className="h-8 w-full min-w-0 flex-1 rounded-lg border border-outline/50 bg-surface-container-low px-2 text-sm text-on-surface"
          value={angle}
          onChange={(event) => setAngle(event.target.value)}
          onBlur={commitAll}
          onKeyDown={(event) => {
            if (event.key === "Enter") {
              commitAll();
            }
          }}
        />
      </div>
      <div className="mt-1 flex items-center justify-end gap-2">
        <button
          type="button"
          className="cad-tool-button h-8"
          onClick={() => void onCancel()}
        >
          {t("common.cancel")}
        </button>
        <button
          type="button"
          className="cad-tool-button cad-tool-button-active h-8"
          onClick={() => {
            commitAll();
            void onCommit();
          }}
        >
          {t("common.ok")}
        </button>
      </div>
    </div>
  );
}
