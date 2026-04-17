import { useState } from "react";

interface BoxFeatureFormProps {
  disabled: boolean;
  onSubmit: (width: number, height: number, depth: number) => Promise<void>;
  variant?: "panel" | "toolbar";
}

export function BoxFeatureForm({
  disabled,
  onSubmit,
  variant = "panel",
}: BoxFeatureFormProps) {
  const [width, setWidth] = useState("20");
  const [height, setHeight] = useState("20");
  const [depth, setDepth] = useState("20");

  async function handleSubmit(event: React.FormEvent<HTMLFormElement>) {
    event.preventDefault();

    await onSubmit(Number(width), Number(height), Number(depth));
  }

  return (
    <section className={variant === "toolbar" ? "px-4 py-4" : "cad-panel px-5 py-5"}>
      <p className="cad-kicker">Create Primitive</p>
      <h2 className={variant === "toolbar" ? "mt-2 font-display text-base tracking-[0.06em] text-on-surface" : "cad-title mt-2"}>
        Add Box Feature
      </h2>
      <form
        onSubmit={(event) => {
          void handleSubmit(event);
        }}
        className={variant === "toolbar" ? "mt-4 grid grid-cols-2 gap-4" : "mt-5 flex flex-wrap items-end gap-4"}
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
        <button
          className={variant === "toolbar" ? "cad-action-primary col-span-2 min-w-[140px]" : "cad-action-primary min-w-[140px]"}
          type="submit"
          disabled={disabled}
        >
          Add Box
        </button>
      </form>
    </section>
  );
}
