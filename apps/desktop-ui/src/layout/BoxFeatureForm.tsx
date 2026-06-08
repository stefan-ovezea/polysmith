import { useTranslation } from "react-i18next";

import { PrimitiveFeatureForm } from "./PrimitiveFeatureForm";

interface BoxFeatureFormProps {
  disabled: boolean;
  onSubmit: (width: number, height: number, depth: number) => Promise<void>;
  variant?: "panel" | "toolbar";
  // When `mode` is "edit", the form is editing an existing feature and
  // the submit button reads "Apply". `initialValues` prefills the inputs;
  // they're tracked by-reference so re-opening the editor on a different
  // feature reseeds the local state.
  mode?: "create" | "edit";
  initialValues?: { width: number; height: number; depth: number };
}

export function BoxFeatureForm({
  disabled,
  onSubmit,
  variant = "panel",
  mode = "create",
  initialValues,
}: BoxFeatureFormProps) {
  const { t } = useTranslation();

  return (
    <PrimitiveFeatureForm
      disabled={disabled}
      variant={variant}
      mode={mode}
      title={mode === "edit" ? t("forms.editBoxFeature") : t("forms.addBoxFeature")}
      submitLabel={mode === "edit" ? t("common.apply") : t("forms.addBox")}
      submitMinWidthClass="min-w-[140px]"
      fields={[
        {
          label: t("forms.width"),
          defaultValue: 20,
          initialValue: initialValues?.width,
        },
        {
          label: t("forms.height"),
          defaultValue: 20,
          initialValue: initialValues?.height,
        },
        {
          label: t("forms.depth"),
          defaultValue: 20,
          initialValue: initialValues?.depth,
        },
      ]}
      onSubmit={async ([width, height, depth]) => {
        await onSubmit(width, height, depth);
      }}
    />
  );
}
