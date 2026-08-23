import { useState, useRef, useCallback } from "react";
import { ConstraintType, SketchTool, ArmedSketchConstraint, type DimensionToolMode } from "@/types";
import { formatHotkey, useAppConfig } from "@/config";
import type { AppHotkeys, CrosshairMode } from "@/config";
import { Dropdown, SplitToolButton } from "@/lib";
import { SketchToolIcon, RectangleIcon, ArcIcon, CircleIcon, PolygonIcon, DimensionIcon } from "./ToolBarIcons";
import { useTranslation } from "react-i18next";
import { HelpPopover } from "@/layout/HelpPopover";
import { helpRegistry } from "@/lib/help-index";
import {
  ArmedConstraintStatus,
  SketchConstraintControls,
} from "./SketchConstraintControls";

interface SketchToolbarProps {
  disabled?: boolean;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  selectedReferenceId: string | null;
  selectedFaceId: string | null;
  armedSketchConstraint: ArmedSketchConstraint;
  // Mirror tool is a contextual modeling action with its own panel, not
  // an armed constraint. The toolbar uses this flag only to
  // light up the Mirror button while the panel is open.
  isMirrorToolOpen: boolean;
  // Arc tool's creation mode. Lifted from App.tsx so the toolbar can
  // render a small segmented control next to the Arc button when the
  // arc tool is active. v1 supports two modes; the toolbar passes
  // the user's choice back through `onSetArcToolMode`.
  arcToolMode: "three_point" | "center_start_end";
  // Rectangle tool's creation mode — split button variant.
  rectangleToolMode: "corner_corner" | "center_point" | "three_point";
  // Circle tool's creation mode — split button variant.
  circleToolMode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines";
  // Polygon tool's creation mode — split button variant.
  polygonToolMode: "circumscribed" | "inscribed" | "edge";

  onStartSketch: () => Promise<void>;
  onFinishSketch: () => Promise<void>;
  onCancelSketchConstraint: () => void;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onArmSketchConstraint: (constraint: ConstraintType) => Promise<void>;
  onStartMirrorTool: () => Promise<void>;
  onSetArcToolMode: (mode: "three_point" | "center_start_end") => void;
  onSetRectangleToolMode: (mode: "corner_corner" | "center_point" | "three_point") => void;
  onSetCircleToolMode: (mode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines") => void;
  onSetPolygonToolMode: (mode: "circumscribed" | "inscribed" | "edge") => void;
  // Dimension tool split button mode
  dimensionToolMode: DimensionToolMode;
  onSetDimensionToolMode: (mode: DimensionToolMode) => void;
  // Body-projection mode for the Project tool: which way a clicked
  // mesh body is projected onto the active sketch (cross-section at
  // the sketch plane vs. silhouette along the plane normal). Shown as
  // a small segmented control next to the Project tool while it is
  // armed.
  bodyProjectionMode: "section" | "silhouette";
  onSetBodyProjectionMode: (mode: "section" | "silhouette") => void;
}

const sketchTools: Array<{
  id: SketchTool;
  labelKey: string;
  hotkey?: keyof AppHotkeys["sketchToolbar"] | "project";
  enabled: boolean;
}> = [
  { id: "select", labelKey: "toolbar.select", enabled: true },
  // The Move tool is NOT on the toolbar (Fusion-style): it is entered
  // via the right-click context menu's "Move/Copy" (or the M hotkey),
  // which arms the tool with the right-clicked entity / selection and
  // shows the persistent manipulator ring.
  { id: "line", labelKey: "toolbar.line", hotkey: "line", enabled: true },
  { id: "dimension", labelKey: "toolbar.dimension", hotkey: "dimension", enabled: true },
  { id: "rectangle", labelKey: "toolbar.rectangle", hotkey: "rectangle", enabled: true },
  { id: "circle", labelKey: "toolbar.circle", hotkey: "circle", enabled: true },
  { id: "polygon", labelKey: "toolbar.polygon", enabled: true },
  { id: "arc", labelKey: "toolbar.arc", enabled: true },
  { id: "fillet", labelKey: "toolbar.fillet", enabled: true },
  { id: "chamfer", labelKey: "toolbar.chamfer", enabled: true },
  { id: "extend", labelKey: "toolbar.extend", enabled: true },
  { id: "offset", labelKey: "toolbar.offset", enabled: true },
  { id: "ellipse", labelKey: "toolbar.ellipse", enabled: true },
  { id: "slot", labelKey: "toolbar.slot", enabled: true },
  { id: "text", labelKey: "toolbar.text", enabled: true },
  { id: "trim", labelKey: "toolbar.trim", hotkey: "trim", enabled: true },
  // Modal Project tool. While active, viewport face / edge / vertex
  // clicks are routed to `project_*_into_sketch` instead of the
  // normal selection. Toggling the button (or pressing P / Esc /
  // picking another tool) deactivates it. See App.tsx click intercept.
  { id: "project", labelKey: "toolbar.project", hotkey: "project", enabled: true },
];

const crosshairOptions: Array<{ id: CrosshairMode; labelKey?: string; label?: string }> = [
  { id: "default", labelKey: "crosshair.default" },
  { id: "viewport-25", label: "25%" },
  { id: "viewport-50", label: "50%" },
  { id: "viewport-75", label: "75%" },
  { id: "infinite", labelKey: "crosshair.infinite" },
];

export function SketchToolbar({
  disabled = false,
  activeSketchPlaneId,
  activeSketchTool,
  selectedReferenceId,
  selectedFaceId,
  armedSketchConstraint,
  isMirrorToolOpen,
  arcToolMode,
  rectangleToolMode,
  circleToolMode,
  polygonToolMode,
  onStartSketch,
  onFinishSketch,
  onCancelSketchConstraint,
  onSetSketchTool,
  onArmSketchConstraint,
  onStartMirrorTool,
  onSetArcToolMode,
  onSetRectangleToolMode,
  onSetCircleToolMode,
  onSetPolygonToolMode,
  dimensionToolMode,
  onSetDimensionToolMode,
  bodyProjectionMode,
  onSetBodyProjectionMode,
}: SketchToolbarProps) {
  const { config, updateConfig } = useAppConfig();
  const { t } = useTranslation();
  const canCreateSketch = Boolean(selectedReferenceId || selectedFaceId);
  const [helpToolId, setHelpToolId] = useState<string | null>(null);
  const [helpAnchor, setHelpAnchor] = useState<HTMLElement | null>(null);
  const helpTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const openHelp = useCallback((toolId: string, el: HTMLElement) => {
    if (helpTimerRef.current) clearTimeout(helpTimerRef.current);
    helpTimerRef.current = setTimeout(() => {
      helpTimerRef.current = null;
      setHelpToolId(toolId);
      setHelpAnchor(el);
    }, 800);
  }, []);

  const closeHelp = useCallback(() => {
    if (helpTimerRef.current) {
      clearTimeout(helpTimerRef.current);
      helpTimerRef.current = null;
    }
    setHelpToolId(null);
    setHelpAnchor(null);
  }, []);

  const helpEntry = helpToolId ? helpRegistry[helpToolId] ?? null : null;
  const toolLabel = (tool: (typeof sketchTools)[number]) => {
    const label = t(tool.labelKey);
    if (!tool.hotkey) {
      return label;
    }
    const binding =
      tool.hotkey === "project"
        ? config.hotkeys.toolbar.project
        : config.hotkeys.sketchToolbar[tool.hotkey];
    return `${label} (${formatHotkey(binding)})`;
  };
  return (
    <>
      <button
        className={
          activeSketchPlaneId
            ? "cad-tool-button cad-tool-button-active"
            : "cad-tool-button"
        }
        data-tooltip={
          activeSketchPlaneId
            ? t("toolbar.finishSketch")
            : `${t("toolbar.createSketch")} (${formatHotkey(config.hotkeys.sketchToolbar.createSketch)})`
        }
        onClick={() => {
          void (activeSketchPlaneId ? onFinishSketch() : onStartSketch());
        }}
        disabled={disabled || (!activeSketchPlaneId && !canCreateSketch)}
      >
        {activeSketchPlaneId ? t("toolbar.finishSketch") : t("toolbar.createSketch")}
      </button>
      {sketchTools
         .filter((t) => t.id !== "rectangle" && t.id !== "arc" && t.id !== "circle" && t.id !== "polygon" && t.id !== "dimension")
        .map((tool) => (
        <span
          key={tool.id}
          onMouseEnter={(e) => openHelp(tool.id, e.currentTarget)}
          onMouseLeave={closeHelp}
          onFocus={(e) => openHelp(tool.id, e.currentTarget)}
          onBlur={closeHelp}
          onClick={closeHelp}
        >
        <button
          className={
            activeSketchPlaneId && activeSketchTool === tool.id
              ? "cad-icon-button cad-icon-tool cad-icon-tool-active h-9 w-9 p-0"
              : "cad-icon-button cad-icon-tool h-9 w-9 p-0"
          }
          data-tooltip={toolLabel(tool)}
          aria-label={t(tool.labelKey)}
          disabled={!activeSketchPlaneId || !tool.enabled}
          onClick={() => {
            if (
              !activeSketchPlaneId ||
              !tool.enabled
            ) {
              return;
            }

            onCancelSketchConstraint();
            // Toggle behaviour for the modal Project tool: clicking it
            // again while it's already active turns it off (returns to
            // Select). For non-modal tools the second click is a
            // no-op because `onSetSketchTool` would fire the same id
            // — harmless but skipped here for clarity.
            if (tool.id === "project" && activeSketchTool === "project") {
              void onSetSketchTool("select");
              return;
            }
            void onSetSketchTool(tool.id);
          }}
        >
          <SketchToolIcon tool={tool.id} />
        </button>
        </span>
      ))}
      <span
        onMouseEnter={(e) => openHelp("rectangle", e.currentTarget)}
        onMouseLeave={closeHelp}
        onFocus={(e) => openHelp("rectangle", e.currentTarget)}
        onBlur={closeHelp}
        onClick={closeHelp}
      >
      <SplitToolButton
        options={[
          { value: "corner_corner" as const, label: t("toolbar.rectangleCornerCorner") },
          { value: "center_point" as const, label: t("toolbar.rectangleCenterPoint") },
          { value: "three_point" as const, label: t("toolbar.rectangleThreePoint") },
        ]}
        value={rectangleToolMode}
        onChange={onSetRectangleToolMode}
        onPrimaryAction={() => {
          onCancelSketchConstraint();
          void onSetSketchTool("rectangle");
        }}
        isActive={activeSketchPlaneId ? activeSketchTool === "rectangle" : false}
        disabled={!activeSketchPlaneId}
        tooltip={toolLabel(
          sketchTools.find((t) => t.id === "rectangle")!,
        )}
        ariaLabel={t("toolbar.rectangle")}
      >
        <RectangleIcon />
      </SplitToolButton>
      </span>
      <span
        onMouseEnter={(e) => openHelp("arc", e.currentTarget)}
        onMouseLeave={closeHelp}
        onFocus={(e) => openHelp("arc", e.currentTarget)}
        onBlur={closeHelp}
        onClick={closeHelp}
      >
      <SplitToolButton
        options={[
          { value: "three_point" as const, label: t("toolbar.arcThreePoint") },
          { value: "center_start_end" as const, label: t("toolbar.arcCenter") },
        ]}
        value={arcToolMode}
        onChange={onSetArcToolMode}
        onPrimaryAction={() => {
          onCancelSketchConstraint();
          void onSetSketchTool("arc");
        }}
        isActive={activeSketchPlaneId ? activeSketchTool === "arc" : false}
        disabled={!activeSketchPlaneId}
        tooltip={toolLabel(
          sketchTools.find((t) => t.id === "arc")!,
        )}
        ariaLabel={t("toolbar.arc")}
      >
        <ArcIcon />
      </SplitToolButton>
      </span>
      <span
        onMouseEnter={(e) => openHelp("circle", e.currentTarget)}
        onMouseLeave={closeHelp}
        onFocus={(e) => openHelp("circle", e.currentTarget)}
        onBlur={closeHelp}
        onClick={closeHelp}
      >
      <SplitToolButton
        options={[
          { value: "center_radius" as const, label: t("toolbar.circleCenterRadius") },
          { value: "two_point" as const, label: t("toolbar.circleTwoPoint") },
          { value: "three_point" as const, label: t("toolbar.circleThreePoint") },
          { value: "tangent_two_lines" as const, label: t("toolbar.circleTangentTwoLines") },
          { value: "tangent_three_lines" as const, label: t("toolbar.circleTangentThreeLines") },
        ]}
        value={circleToolMode}
        onChange={onSetCircleToolMode}
        onPrimaryAction={() => {
          onCancelSketchConstraint();
          void onSetSketchTool("circle");
        }}
        isActive={activeSketchPlaneId ? activeSketchTool === "circle" : false}
        disabled={!activeSketchPlaneId}
        tooltip={toolLabel(
          sketchTools.find((t) => t.id === "circle")!,
        )}
        ariaLabel={t("toolbar.circle")}
      >
        <CircleIcon />
      </SplitToolButton>
      </span>
      <span
        onMouseEnter={(e) => openHelp("polygon", e.currentTarget)}
        onMouseLeave={closeHelp}
        onFocus={(e) => openHelp("polygon", e.currentTarget)}
        onBlur={closeHelp}
        onClick={closeHelp}
      >
      <SplitToolButton
        options={[
          { value: "circumscribed" as const, label: t("toolbar.polygonCircumscribed") },
          { value: "inscribed" as const, label: t("toolbar.polygonInscribed") },
          { value: "edge" as const, label: t("toolbar.polygonEdge") },
        ]}
        value={polygonToolMode}
        onChange={onSetPolygonToolMode}
        onPrimaryAction={() => {
          onCancelSketchConstraint();
          void onSetSketchTool("polygon");
        }}
        isActive={activeSketchPlaneId ? activeSketchTool === "polygon" : false}
        disabled={!activeSketchPlaneId}
        tooltip={toolLabel(
          sketchTools.find((t) => t.id === "polygon")!,
        )}
        ariaLabel={t("toolbar.polygon")}
      >
        <PolygonIcon />
      </SplitToolButton>
      </span>
      <span
        onMouseEnter={(e) => openHelp("dimension", e.currentTarget)}
        onMouseLeave={closeHelp}
        onFocus={(e) => openHelp("dimension", e.currentTarget)}
        onBlur={closeHelp}
        onClick={closeHelp}
      >
      <SplitToolButton
        options={[
          { value: "auto" as const, label: t("toolbar.dimensionAuto") },
          { value: "linear" as const, label: t("toolbar.dimensionLinear") },
          { value: "aligned" as const, label: t("toolbar.dimensionAligned") },
          { value: "angular" as const, label: t("toolbar.dimensionAngular") },
          { value: "radius" as const, label: t("toolbar.dimensionRadius") },
          { value: "diameter" as const, label: t("toolbar.dimensionDiameter") },
          { value: "arc_length" as const, label: t("toolbar.dimensionArcLength") },
        ]}
        value={dimensionToolMode}
        onChange={onSetDimensionToolMode}
        onPrimaryAction={() => {
          onCancelSketchConstraint();
          void onSetSketchTool("dimension");
        }}
        isActive={activeSketchPlaneId ? activeSketchTool === "dimension" : false}
        disabled={!activeSketchPlaneId}
        tooltip={toolLabel(
          sketchTools.find((t) => t.id === "dimension")!,
        )}
        ariaLabel={t("toolbar.dimension")}
      >
        <DimensionIcon />
      </SplitToolButton>
      </span>
      {activeSketchTool === "project" ? (
        <>
          <button
            type="button"
            className={
              bodyProjectionMode === "section"
                ? "cad-ribbon-action cad-ribbon-action-primary"
                : "cad-ribbon-action"
            }
            data-tooltip={t("toolbar.projectSection")}
            onClick={() => onSetBodyProjectionMode("section")}
          >
            {t("toolbar.projectSection")}
          </button>
          <button
            type="button"
            className={
              bodyProjectionMode === "silhouette"
                ? "cad-ribbon-action cad-ribbon-action-primary"
                : "cad-ribbon-action"
            }
            data-tooltip={t("toolbar.projectSilhouette")}
            onClick={() => onSetBodyProjectionMode("silhouette")}
          >
            {t("toolbar.projectSilhouette")}
          </button>
        </>
      ) : null}
      <div className="h-8 w-px bg-white/10" />
      <SketchConstraintControls
        activeSketchPlaneId={activeSketchPlaneId}
        armedSketchConstraint={armedSketchConstraint}
        isMirrorToolOpen={isMirrorToolOpen}
        onArmSketchConstraint={onArmSketchConstraint}
        onStartMirrorTool={onStartMirrorTool}
      />
      <div className="h-8 w-px bg-white/10" />
      <Dropdown
        label={t("toolbar.sketchCrosshair")}
        className="w-[104px]"
        buttonClassName="h-9"
        value={config.viewport.crosshair}
        options={crosshairOptions.map((option) => ({
          value: option.id,
          label: option.labelKey ? t(option.labelKey) : option.label,
        }))}
        onChange={(crosshair) => {
          updateConfig((current) => ({
            ...current,
            viewport: {
              ...current.viewport,
              crosshair,
            },
          }));
        }}
      />
      <ArmedConstraintStatus armedSketchConstraint={armedSketchConstraint} />
      <HelpPopover
        entry={helpEntry}
        anchor={helpAnchor}
        onClose={closeHelp}
      />
    </>
  );
}
