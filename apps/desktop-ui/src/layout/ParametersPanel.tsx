import { useState, useRef, useEffect, useCallback, useMemo } from "react";
import { useTranslation } from "react-i18next";
import { useCadCoreStore } from "@/state";
import { useCadCore } from "@/hooks";
import { useAppConfig } from "@/config";
import { mmToDisplay, displayToMm } from "@/utils/units";
import { Dropdown } from "@/lib";
import type { DropdownOption } from "@/lib";
import type { ParameterEntry } from "@/types";

interface EditingRow {
  index: number;
  name: string;
  expression: string;
  kind: "length" | "angle";
  isNew: boolean;
}

const EDIT_INPUT_CLASS =
  "h-8 w-full rounded-lg border border-surface-high/70 bg-surface-container px-2 text-xs outline-none focus:border-primary-soft";

export function ParametersPanel({ onClose }: { onClose?: () => void }) {
  const { t } = useTranslation();
  const document = useCadCoreStore((s) => s.document);
  const { config } = useAppConfig();
  const { addParameter, updateParameter, deleteParameter } = useCadCore();

  const [editing, setEditing] = useState<EditingRow | null>(null);
  const nameRef = useRef<HTMLInputElement | null>(null);
  const exprRef = useRef<HTMLInputElement | null>(null);
  // Stable ref so commitEdit always reads the latest kind even when
  // a Dropdown onChange fires and the expression field blur races
  // against the React state update (commitEdit's closure would
  // otherwise see the stale kind).
  const kindRef = useRef<"length" | "angle">("length");

  const parameters: ParameterEntry[] = document?.parameters ?? [];

  useEffect(() => {
    if (!onClose) {
      return undefined;
    }
    const handleEscape = (event: KeyboardEvent) => {
      if (event.key !== "Escape") {
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      onClose();
    };
    window.addEventListener("keydown", handleEscape);
    return () => {
      window.removeEventListener("keydown", handleEscape);
    };
  }, [onClose]);

  // Detect a plain numeric expression (no letters, no operators) so we
  // can convert from the display unit to mm. Formulas like "width * 2"
  // are sent as-is — the user is responsible for unit consistency there.
  const PLAIN_NUMBER_RE = /^[0-9.,]+$/;

  const commitEdit = useCallback(async (overrideKind?: "length" | "angle") => {
    if (!editing) return;
    const { index, name, expression, isNew } = editing;
    const kind = overrideKind ?? kindRef.current;

    if (!name.trim()) {
      setEditing(null);
      return;
    }

    const rawExpression = expression.trim();
    // Don't commit without an expression — silently keep the row open
    if (!rawExpression) {
      return;
    }

    // When creating a new length parameter in inch mode, convert a
    // plain numeric expression from inches to mm so the stored value
    // is always in the core's native unit.  Once stored, the expression
    // lives in mm — subsequent edits in any display mode are already in
    // mm and should not be re-converted.  Angle parameters are unitless.
    let finalExpression = rawExpression;
    if (
      isNew &&
      kind === "length" &&
      config.displayUnits === "in" &&
      PLAIN_NUMBER_RE.test(rawExpression)
    ) {
      const normalized = rawExpression.replace(",", ".");
      const num = parseFloat(normalized);
      if (!isNaN(num) && num > 0) {
        finalExpression = String(displayToMm(num, "in"));
      }
    }

    try {
      if (isNew) {
        await addParameter(name.trim(), finalExpression, kind);
      } else {
        const prev = parameters[index];
        if (
          prev &&
          prev.name === name.trim() &&
          prev.expression === rawExpression &&
          prev.kind === kind
        ) {
          setEditing(null);
          return;
        }
        await updateParameter(name.trim(), finalExpression, kind);
      }
    } catch (err) {
      console.error("[ParametersPanel] commitEdit failed:", err);
    }
    setEditing(null);
  }, [editing, parameters, config.displayUnits, addParameter, updateParameter]);

  // Auto-focus the name field only when entering edit mode (editing
  // transitions from null to a non-null value). Tracking via a ref
  // avoids re-focus (and thus blur on the expression field) on every
  // keystroke, which would otherwise trigger a premature commit.
  const prevEditingRef = useRef<EditingRow | null>(null);
  useEffect(() => {
    if (editing && editing !== prevEditingRef.current) {
      // Only focus when switching to a different row or entering edit
      if (!prevEditingRef.current || prevEditingRef.current.index !== editing.index || prevEditingRef.current.isNew !== editing.isNew) {
        nameRef.current?.focus();
      }
    }
    prevEditingRef.current = editing;
  }, [editing]);

  const startAdd = () => {
    kindRef.current = "length";
    setEditing({ index: -1, name: "", expression: "", kind: "length", isNew: true });
  };

  const startEdit = (index: number) => {
    const p = parameters[index];
    if (!p) return;
    kindRef.current = p.kind;
    setEditing({
      index,
      name: p.name,
      expression: p.expression,
      kind: p.kind,
      isNew: false,
    });
  };

  const KIND_OPTIONS = useMemo<DropdownOption<"length" | "angle">[]>(
    () => [
      { value: "length", label: t("parameters.kindLength") },
      { value: "angle", label: t("parameters.kindAngle") },
    ],
    [t],
  );

  const displayValue = (param: ParameterEntry): string => {
    if (param.kind === "angle") {
      return param.resolved_value.toFixed(2) + "\u00b0";
    }
    const display = mmToDisplay(param.resolved_value, config.displayUnits);
    const prec = config.displayUnits === "in" ? 3 : 2;
    return display.toFixed(prec) + " " + config.displayUnits;
  };

  const handleKeyDown = (
    e: React.KeyboardEvent<HTMLInputElement>,
  ) => {
    if (e.key === "Enter") {
      e.preventDefault();
      commitEdit();
    }
    if (e.key === "Escape") {
      setEditing(null);
    }
    // Tab advances natively to the next focusable element.
  };

  const renderEditActions = () => (
    <div className="flex items-center justify-end gap-2">
      <button
        type="button"
        className="cad-ribbon-action h-8 rounded-lg px-3 py-1 text-xs"
        onClick={() => void commitEdit()}
      >
        {t("parameters.save")}
      </button>
      <button
        type="button"
        className="h-8 rounded-lg px-3 py-1 text-xs text-on-surface-muted hover:bg-surface-bright hover:text-on-surface"
        onClick={() => setEditing(null)}
      >
        {t("parameters.cancel")}
      </button>
    </div>
  );

  const renderEditingCells = (placeholders: boolean) =>
    editing ? (
      <>
        <td className="rounded-l-lg px-2 py-2">
          <input
            ref={nameRef}
            className={EDIT_INPUT_CLASS}
            placeholder={placeholders ? t("parameters.name") : undefined}
            value={editing.name}
            onChange={(e) => setEditing({ ...editing, name: e.target.value })}
            onKeyDown={(e) => handleKeyDown(e)}
          />
        </td>
        <td className="px-2 py-2">
          <input
            ref={exprRef}
            className={EDIT_INPUT_CLASS}
            placeholder={placeholders ? "e.g. 50 or width * 2" : undefined}
            value={editing.expression}
            onChange={(e) =>
              setEditing({
                ...editing,
                expression: e.target.value,
              })
            }
            onKeyDown={(e) => handleKeyDown(e)}
          />
        </td>
        <td className="px-2 py-2">
          <Dropdown
            value={editing.kind}
            options={KIND_OPTIONS}
            label={t("parameters.kind")}
            onChange={(kind) => {
              kindRef.current = kind;
              setEditing({ ...editing, kind });
            }}
          />
        </td>
      </>
    ) : null;

  return (
    <>
      {/* Invisible backdrop: clicking anywhere outside the panel closes it */}
      {onClose ? (
        <div
          className="fixed inset-0 z-40"
          onPointerDown={onClose}
        />
      ) : null}
      <section
        className="pointer-events-auto cad-floating-panel relative z-50 w-[560px] px-5 py-5"
        style={{ background: "var(--cad-panel-bg)" }}
      >
        <div className="flex items-center justify-between gap-4">
          <p className="cad-kicker">{t("parameters.title")}</p>
          <button
            type="button"
            className="cad-ribbon-action h-8 rounded-lg px-3 py-1 text-xs"
            onClick={startAdd}
          >
            + {t("parameters.addParameter")}
          </button>
        </div>

      <div className="mt-4 max-h-[360px] overflow-visible rounded-xl p-1.5 ring-1 ring-inset ring-surface-high/40">
        <table className="w-full border-separate border-spacing-y-0.5 text-xs">
          <thead>
            <tr className="text-on-surface-dim text-left">
              <th className="px-2 pb-2 font-medium">
                {t("parameters.name")}
              </th>
              <th className="px-2 pb-2 font-medium">
                {t("parameters.expression")}
              </th>
              <th className="px-2 pb-2 font-medium">
                {t("parameters.kind")}
              </th>
              <th className="px-2 pb-2 font-medium">
                {t("parameters.value")}
              </th>
              <th className="w-28 px-2 pb-2" />
            </tr>
          </thead>
          <tbody>
            {parameters.map((param, index) => {
              const isEditing =
                editing !== null &&
                !editing.isNew &&
                editing.index === index;

              return (
                <tr
                  key={param.name}
                  className={`group rounded-lg ${
                    param.has_error ? "text-danger" : "text-on-surface"
                  }`}
                >
                  {isEditing ? (
                    renderEditingCells(false)
                  ) : (
                    <>
                      <td
                        className="cursor-pointer rounded-l-lg px-2 py-2 font-mono hover:bg-surface-container"
                        onClick={() => startEdit(index)}
                      >
                        {param.name}
                      </td>
                      <td
                        className="cursor-pointer px-2 py-2 font-mono hover:bg-surface-container"
                        onClick={() => startEdit(index)}
                      >
                        {param.expression}
                      </td>
                      <td
                        className="cursor-pointer px-2 py-2 font-mono hover:bg-surface-container"
                        onClick={() => startEdit(index)}
                      >
                        {param.kind === "angle"
                          ? t("parameters.kindAngle")
                          : t("parameters.kindLength")}
                      </td>
                    </>
                  )}
                  <td className="px-2 py-2 font-mono">
                    {param.has_error ? (
                      <span
                        className="text-danger"
                        title={param.error_message}
                      >
                        {param.error_message || "Error"}
                      </span>
                    ) : (
                      displayValue(param)
                    )}
                  </td>
                  <td className="rounded-r-lg px-2 py-2">
                    {isEditing ? (
                      renderEditActions()
                    ) : (
                      <button
                        type="button"
                        className="invisible ml-auto block rounded-lg px-2 py-1 text-on-surface-dim hover:bg-surface-container hover:text-danger group-hover:visible"
                        title={t("parameters.deleteParameter")}
                        onClick={() => void deleteParameter(param.name)}
                      >
                        ×
                      </button>
                    )}
                  </td>
                </tr>
              );
            })}

            {editing?.isNew ? (
              <tr>
                {renderEditingCells(true)}
                <td className="px-2 py-2" />
                <td className="rounded-r-lg px-2 py-2">
                  {renderEditActions()}
                </td>
              </tr>
            ) : null}
          </tbody>
        </table>
      </div>
      </section>
    </>
  );
}
