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
  { kind: "midpoint", labelKey: "toolbar.midpoint" },
  { kind: "equal_length", labelKey: "toolbar.equalLength" },
  { kind: "perpendicular", labelKey: "toolbar.perpendicular" },
  { kind: "parallel", labelKey: "toolbar.parallel" },
  { kind: "collinear", labelKey: "toolbar.collinear" },
  { kind: "tangent", labelKey: "toolbar.tangent" },
  { kind: "tangent_pair", labelKey: "toolbar.tangentPair" },
  { kind: "symmetric", labelKey: "toolbar.symmetric" },
  { kind: "fixed", labelKey: "toolbar.fix" },
];

interface SketchConstraintControlsProps {
  activeSketchPlaneId: string | null;
  armedSketchConstraint: ArmedSketchConstraint;
  isMirrorToolOpen: boolean;
  // Transform/Array panel (Move/Copy) — the Array toolbar button
  // lights up while that panel is open.
  isArrayPanelOpen: boolean;
  onArmSketchConstraint: (constraint: ConstraintType) => Promise<void>;
  onStartMirrorTool: () => Promise<void>;
  onStartArrayTool: () => void;
}

export function SketchConstraintControls({
  activeSketchPlaneId,
  armedSketchConstraint,
  isMirrorToolOpen,
  isArrayPanelOpen,
  onArmSketchConstraint,
  onStartMirrorTool,
  onStartArrayTool,
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
      {/* Array opens the same Transform/Array floating panel as the
          right-click Move/Copy entry — the toolbar button is just the
          discoverable path to the same tool. */}
      <button
        className={constraintButtonClass(activeSketchPlaneId, isArrayPanelOpen)}
        data-tooltip={t("toolbar.array")}
        aria-label={t("toolbar.array")}
        disabled={disabled}
        onClick={onStartArrayTool}
      >
        <ConstraintIcon kind="array" />
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

  if (armedSketchConstraint.kind === "fixed") {
    return t("constraints.fixClickPoint");
  }

  // Midpoint (S1) is point-then-line, so the hint changes after the first pick.
  if (armedSketchConstraint.kind === "midpoint") {
    return armedSketchConstraint.firstPointId
      ? t("constraints.midpointClickLine")
      : t("constraints.midpointClickPoint");
  }

  // Tangent (S1): line first, then a circle/arc.
  if (armedSketchConstraint.kind === "tangent") {
    return armedSketchConstraint.firstLineId
      ? t("constraints.tangentClickCurve")
      : t("constraints.tangentClickLine");
  }

  // Tangent pair (S1): two curves in either order.
  if (armedSketchConstraint.kind === "tangent_pair") {
    return armedSketchConstraint.firstEntityId
      ? t("constraints.tangentPairClickSecond")
      : t("constraints.tangentPairClickFirst");
  }

  // Symmetric (S1): point, point, axis.
  if (armedSketchConstraint.kind === "symmetric") {
    if (!armedSketchConstraint.firstPointId) {
      return t("constraints.symmetricClickFirstPoint");
    }
    if (!armedSketchConstraint.secondPointId) {
      return t("constraints.symmetricClickSecondPoint");
    }
    return t("constraints.symmetricClickAxis");
  }

  if (
    armedSketchConstraint.kind === "equal_length" ||
    armedSketchConstraint.kind === "perpendicular" ||
    armedSketchConstraint.kind === "parallel" ||
    armedSketchConstraint.kind === "collinear"
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
  kind: "equal_length" | "perpendicular" | "parallel" | "collinear",
  t: (key: string) => string,
) {
  if (kind === "equal_length") {
    return t("toolbar.equalLength");
  }
  if (kind === "perpendicular") {
    return t("toolbar.perpendicular");
  }
  if (kind === "collinear") {
    return t("toolbar.collinear");
  }
  return t("toolbar.parallel");
}
