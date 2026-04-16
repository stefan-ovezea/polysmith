import { useEffect, useState } from "react";
import type { FeatureEntry } from "../types/ipc";

interface SelectedBoxEditorProps {
  feature: FeatureEntry | null;
  disabled: boolean;
  onSubmit: (featureId: string, width: number, height: number, depth: number) => Promise<void>;
  onRename: (featureId: string, name: string) => Promise<void>;
  onDelete: (featureId: string) => Promise<void>;
}

export function SelectedBoxEditor({
  feature,
  disabled,
  onSubmit,
  onRename,
  onDelete,
}: SelectedBoxEditorProps) {
  const [name, setName] = useState("");
  const [width, setWidth] = useState("");
  const [height, setHeight] = useState("");
  const [depth, setDepth] = useState("");

  useEffect(() => {
    if (!feature?.box_parameters) {
      setName(feature?.name ?? "");
      setWidth("");
      setHeight("");
      setDepth("");
      return;
    }

    setName(feature.name);
    setWidth(String(feature.box_parameters.width));
    setHeight(String(feature.box_parameters.height));
    setDepth(String(feature.box_parameters.depth));
  }, [feature]);

  if (!feature) {
    return (
      <section className="cad-panel px-5 py-5">
        <p className="cad-kicker">Inspector</p>
        <h2 className="cad-title mt-2">Selected Feature</h2>
        <p className="mt-4 text-sm text-on-surface-muted">
          Select a feature from the browser, viewport, or timeline to inspect it.
        </p>
      </section>
    );
  }

  if (!feature.box_parameters) {
    return (
      <section className="cad-panel px-5 py-5">
        <p className="cad-kicker">Inspector</p>
        <h2 className="cad-title mt-2">Selected Feature</h2>
        <div className="mt-4 space-y-3 text-sm">
          <p className="text-on-surface-muted">
            The selected feature is not editable as a box.
          </p>
          <button
            className="cad-action-ghost"
            onClick={() => {
              void onRename(feature.feature_id, name || feature.name);
            }}
            disabled={disabled || name.trim().length === 0}
          >
            Rename Feature
          </button>
        </div>
      </section>
    );
  }

  return (
    <section className="cad-panel px-5 py-5">
      <div className="flex items-start justify-between gap-4">
        <div>
          <p className="cad-kicker">Inspector</p>
          <h2 className="cad-title mt-2">Edit Selected Box</h2>
        </div>
        <button
          className="cad-action-ghost text-danger"
          onClick={() => {
            void onDelete(feature.feature_id);
          }}
          disabled={disabled}
        >
          Delete
        </button>
      </div>

      <div className="mt-5">
        <label className="text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Feature Name
          <div className="mt-2 flex gap-3">
            <input
              className="cad-input"
              value={name}
              onChange={(event) => {
                setName(event.target.value);
              }}
              disabled={disabled}
            />
            <button
              className="cad-action-ghost shrink-0"
              type="button"
              onClick={() => {
                void onRename(feature.feature_id, name);
              }}
              disabled={disabled || name.trim().length === 0}
            >
              Rename
            </button>
          </div>
        </label>
      </div>

      <form
        onSubmit={(event) => {
          event.preventDefault();
          void onSubmit(feature.feature_id, Number(width), Number(height), Number(depth));
        }}
        className="mt-5 flex flex-wrap items-end gap-4"
      >
        <label className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Width
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={width}
            onChange={(event) => {
              setWidth(event.target.value);
            }}
            disabled={disabled}
          />
        </label>
        <label className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Height
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={height}
            onChange={(event) => {
              setHeight(event.target.value);
            }}
            disabled={disabled}
          />
        </label>
        <label className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted">
          Depth
          <input
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={depth}
            onChange={(event) => {
              setDepth(event.target.value);
            }}
            disabled={disabled}
          />
        </label>
        <button className="cad-action-primary min-w-[140px]" type="submit" disabled={disabled}>
          Update Box
        </button>
      </form>
    </section>
  );
}
