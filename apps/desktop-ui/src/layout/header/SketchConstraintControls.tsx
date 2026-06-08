import { useTranslation } from "react-i18next";
import type { ArmedSketchConstraint, ConstraintType } from "@/types";
import { ConstraintIcon } from "./ToolBarIcons";

const constraintButtons: Array<{
  kind: ConstraintType;
  labelKey: string;
}> = [
  { kind: "horizontal", labelKey: "toolbar.horizontal" },
  { kind: "vertical", labelKey: "toolbar.vertical" },
  { kind: "clear", labelKey: "toolbar.clearConstraint" },
  { kind: "coincident", labelKey: "toolbar.coincident" },
  { kind: "equal_length", labelKey: "toolbar.equalLength" },
  { kind: "perpendicular", labelKey: "toolbar.perpendicular" },
  { kind: "parallel", labelKey: "toolbar.parallel" },
];

interface SketchConstraintControlsProps {
  activeSketchPlaneId: string | null;
  armedSketchConstraint: ArmedSketchConstraint;
  isMirrorToolOpen: boolean;
  onArmSketchConstraint: (constraint: ConstraintType) => Promise<void>;
  onStartMirrorTool: () => Promise<void>;
}

export function SketchConstraintControls({
  activeSketchPlaneId,
  armedSketchConstraint,
  isMirrorToolOpen,
  onArmSketchConstraint,
  onStartMirrorTool,
}: SketchConstraintControlsProps) {
  const { t } = useTranslation();
  const disabled = !activeSketchPlaneId;

  return (
    <>
      {constraintButtons.map((button) => (
        <button
          key={button.kind}
          className={constraintButtonClass(
            activeSketchPlaneId,
            armedSketchConstraint?.kind === button.kind,
          )}
          data-tooltip={t(button.labelKey)}
          aria-label={t(button.labelKey)}
          disabled={disabled}
          onClick={() => {
            void onArmSketchConstraint(button.kind);
          }}
        >
          <ConstraintIcon kind={button.kind} />
        </button>
      ))}
      <button
        className={constraintButtonClass(activeSketchPlaneId, isMirrorToolOpen)}
        data-tooltip={t("toolbar.mirror")}
        aria-label={t("toolbar.mirror")}
        disabled={disabled}
        onClick={() => {
          void onStartMirrorTool();
        }}
      >
        <ConstraintIcon kind="mirror" />
      </button>
    </>
  );
}

export function ArmedConstraintStatus({
  armedSketchConstraint,
}: {
  armedSketchConstraint: ArmedSketchConstraint;
}) {
  const { t } = useTranslation();

  if (!armedSketchConstraint) {
    return null;
  }

  return (
    <p className="text-xs uppercase tracking-[0.14em] text-on-surface-dim">
      {armedConstraintStatusText(armedSketchConstraint, t)}
    </p>
  );
}

function constraintButtonClass(
  activeSketchPlaneId: string | null,
  isActive: boolean,
) {
  return activeSketchPlaneId && isActive
    ? "cad-icon-button cad-icon-tool cad-icon-tool-active h-9 w-9 p-0"
    : "cad-icon-button cad-icon-tool h-9 w-9 p-0";
}

function armedConstraintStatusText(
  armedSketchConstraint: NonNullable<ArmedSketchConstraint>,
  t: (key: string, options?: Record<string, unknown>) => string,
) {
  if (armedSketchConstraint.kind === "coincident") {
    return armedSketchConstraint.firstPointId
      ? t("constraints.coincidentSecondPointColon")
      : t("constraints.coincidentFirstPointColon");
  }

  if (
    armedSketchConstraint.kind === "equal_length" ||
    armedSketchConstraint.kind === "perpendicular" ||
    armedSketchConstraint.kind === "parallel"
  ) {
    const label = constraintRelationLabel(armedSketchConstraint.kind, t);
    return armedSketchConstraint.firstLineId
      ? t("constraints.lineSecondColon", { label })
      : t("constraints.lineFirstColon", { label });
  }

  return t("constraints.clickLineColon", {
    kind: armedSketchConstraint.kind,
  });
}

function constraintRelationLabel(
  kind: "equal_length" | "perpendicular" | "parallel",
  t: (key: string) => string,
) {
  if (kind === "equal_length") {
    return t("toolbar.equalLength");
  }
  if (kind === "perpendicular") {
    return t("toolbar.perpendicular");
  }
  return t("toolbar.parallel");
}
