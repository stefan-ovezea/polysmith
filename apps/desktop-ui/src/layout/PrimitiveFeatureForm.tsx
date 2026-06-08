import {
  useEffect,
  useMemo,
  useState,
  type FormEvent,
  type ReactNode,
} from "react";
import { useTranslation } from "react-i18next";

type PrimitiveFormVariant = "panel" | "toolbar";
type PrimitiveFormMode = "create" | "edit";

export interface PrimitiveNumberField {
  label: ReactNode;
  defaultValue: number;
  initialValue?: number;
}

interface PrimitiveFeatureFormProps {
  disabled: boolean;
  variant: PrimitiveFormVariant;
  mode: PrimitiveFormMode;
  title: ReactNode;
  submitLabel: ReactNode;
  fields: PrimitiveNumberField[];
  submitMinWidthClass: string;
  onSubmit: (values: number[]) => Promise<void>;
}

export function PrimitiveFeatureForm({
  disabled,
  variant,
  mode,
  title,
  submitLabel,
  fields,
  submitMinWidthClass,
  onSubmit,
}: PrimitiveFeatureFormProps) {
  const { t } = useTranslation();
  const [values, setValues] = useState(() =>
    fields.map((field) => String(field.initialValue ?? field.defaultValue)),
  );
  const initialValueKey = useMemo(
    () =>
      fields
        .map((field) =>
          field.initialValue === undefined ? "" : String(field.initialValue),
        )
        .join("|"),
    [fields],
  );

  useEffect(() => {
    if (fields.every((field) => field.initialValue === undefined)) {
      return;
    }
    setValues(
      fields.map((field) => String(field.initialValue ?? field.defaultValue)),
    );
  }, [initialValueKey]);

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    await onSubmit(values.map(Number));
  }

  return (
    <section
      className={variant === "toolbar" ? "px-4 py-4" : "cad-panel px-5 py-5"}
    >
      <p className="cad-kicker">
        {mode === "edit" ? t("forms.editFeature") : t("forms.createPrimitive")}
      </p>
      <h2
        className={
          variant === "toolbar"
            ? "mt-2 font-display text-base tracking-[0.06em] text-on-surface"
            : "cad-title mt-2"
        }
      >
        {title}
      </h2>
      <form
        onSubmit={(event) => {
          void handleSubmit(event);
        }}
        className={
          variant === "toolbar"
            ? "mt-4 grid grid-cols-2 gap-4"
            : "mt-5 flex flex-wrap items-end gap-4"
        }
      >
        {fields.map((field, index) => (
          <label
            key={index}
            className="min-w-[96px] flex-1 text-xs uppercase tracking-[0.2em] text-on-surface-muted"
          >
            {field.label}
            <input
              className="cad-input mt-2"
              type="number"
              min="0.01"
              step="0.01"
              value={values[index] ?? ""}
              onChange={(event) => {
                const next = [...values];
                next[index] = event.target.value;
                setValues(next);
              }}
              disabled={disabled}
            />
          </label>
        ))}
        <button
          className={
            variant === "toolbar"
              ? `cad-action-primary col-span-2 ${submitMinWidthClass}`
              : `cad-action-primary ${submitMinWidthClass}`
          }
          type="submit"
          disabled={disabled}
        >
          {submitLabel}
        </button>
      </form>
    </section>
  );
}
