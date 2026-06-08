import { useTranslation } from "react-i18next";

import { PrimitiveFeatureForm } from "./PrimitiveFeatureForm";

interface CylinderFeatureFormProps {
  disabled: boolean;
  onSubmit: (radius: number, height: number) => Promise<void>;
  variant?: "panel" | "toolbar";
  // "edit" turns this into a parameter editor for an existing cylinder
  // feature. `initialValues` prefills the inputs; they reseed when the
  // values change so swapping the edited feature works without
  // remounting the form.
  mode?: "create" | "edit";
  initialValues?: { radius: number; height: number };
}

export function CylinderFeatureForm({
  disabled,
  onSubmit,
  variant = "panel",
  mode = "create",
  initialValues,
}: CylinderFeatureFormProps) {
  const { t } = useTranslation();

  return (
    <PrimitiveFeatureForm
      disabled={disabled}
      variant={variant}
      mode={mode}
      title={
        mode === "edit"
          ? t("forms.editCylinderFeature")
          : t("forms.addCylinderFeature")
      }
      submitLabel={mode === "edit" ? t("common.apply") : t("forms.addCylinder")}
      submitMinWidthClass="min-w-[160px]"
      fields={[
        {
          label: t("forms.radius"),
          defaultValue: 10,
          initialValue: initialValues?.radius,
        },
        {
          label: t("forms.height"),
          defaultValue: 24,
          initialValue: initialValues?.height,
        },
      ]}
      onSubmit={async ([radius, height]) => {
        await onSubmit(radius, height);
      }}
    />
  );
}
