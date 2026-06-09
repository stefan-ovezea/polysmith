import { useCallback, useEffect, useRef, useState } from "react";
import type { ReactNode } from "react";
import { useTranslation } from "react-i18next";
import {
  ConstraintType,
  SketchTool,
  ArmedSketchConstraint,
  type SelectionFilterUpdate,
} from "@/types";
import { useAppConfig } from "@/config";
import { SketchToolbar } from "./SketchToolbar";
import { SketchDofBadge } from "./SketchDofBadge";
import { CreateToolbar } from "./CreateToolbar";
import type { CreateToolbarProps } from "./CreateToolbar";
import { ModifyToolbar } from "./ModifyToolbar";
import { ConstructToolbar } from "./ConstructToolbar";
import type { ConstructToolbarProps } from "./ConstructToolbar";
import type { CamOperationType } from "./CamToolbar";
import { CamMillingToolbar } from "./CamMillingToolbar";
import { CamTurningToolbar } from "./CamTurningToolbar";
import { CamPrintingToolbar } from "./CamPrintingToolbar";
import { CamCuttingToolbar } from "./CamCuttingToolbar";
import { ParametersPanel } from "../ParametersPanel";
import { SelectionFilterPanel } from "../SelectionFilterPanel";
import { readStoredFilter, writeStoredFilter } from "../selectionFilterState";

const workspaces = ["create", "modify", "construct", "sketch"] as const;
const camWorkspaces = ["milling", "turning", "printing", "cutting"] as const;
type CamWorkspace = (typeof camWorkspaces)[number];
type WorkspaceView = "cad" | "slicer" | "cam";
type AppHeaderCreateToolbarProps = Omit<
  CreateToolbarProps,
  "disabled" | "openMenu" | "setOpenMenu"
>;
type AppHeaderConstructToolbarProps = Omit<
  ConstructToolbarProps,
  "disabled"
>;

interface MenuDropdownItem {
  label: string;
  disabled?: boolean;
  onSelect: () => void;
}

interface MenuDropdownProps {
  label: string;
  icon?: ReactNode;
  disabled?: boolean;
  align?: "start" | "end";
  items: MenuDropdownItem[];
  onOpenChange?: (isOpen: boolean) => void;
}

function MenuDropdown({
  label,
  icon,
  disabled,
  align = "end",
  items,
  onOpenChange,
}: MenuDropdownProps) {
  const [isOpen, setIsOpen] = useState(false);

  // Notify parent when dropdown opens/closes so the native Orca window
  // can be hidden/shown to avoid covering the dropdown popup.
  useEffect(() => {
    onOpenChange?.(isOpen);
  }, [isOpen, onOpenChange]);
  const containerRef = useRef<HTMLDivElement | null>(null);
  const alignmentClass = align === "start" ? "left-0" : "right-0";

  useEffect(() => {
    if (!isOpen) {
      return undefined;
    }
    function handleOutside(event: MouseEvent) {
      if (
        containerRef.current &&
        event.target instanceof Node &&
        !containerRef.current.contains(event.target)
      ) {
        setIsOpen(false);
      }
    }
    function handleKey(event: KeyboardEvent) {
      if (event.key === "Escape") {
        setIsOpen(false);
      }
    }
    window.addEventListener("mousedown", handleOutside);
    window.addEventListener("keydown", handleKey);
    return () => {
      window.removeEventListener("mousedown", handleOutside);
      window.removeEventListener("keydown", handleKey);
    };
  }, [isOpen]);

  return (
    <div ref={containerRef} className="relative">
      <button
        type="button"
        className="cad-ribbon-action"
        disabled={disabled}
        onClick={() => setIsOpen((current) => !current)}
        aria-label={label}
        title={label}
      >
        {icon ?? label}
        <span
          aria-hidden
          className={
            icon ? "ml-1 text-on-surface-dim" : "ml-1.5 text-on-surface-dim"
          }
        >
          ▾
        </span>
      </button>
      {isOpen ? (
        <div
          className={`cad-context-menu absolute ${alignmentClass} top-[calc(100%+6px)] z-30 min-w-[180px] rounded-xl p-1 shadow-[0_8px_24px_rgba(0,0,0,0.5)] backdrop-blur-xl`}
        >
          {items.map((item) => (
            <button
              key={item.label}
              type="button"
              className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10 disabled:opacity-40 disabled:hover:bg-transparent"
              disabled={item.disabled}
              onClick={() => {
                setIsOpen(false);
                item.onSelect();
              }}
            >
              {item.label}
            </button>
          ))}
        </div>
      ) : null}
    </div>
  );
}

function FileMenuIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M6 3.5h8l4 4v13H6Z" />
      <path d="M14 3.5v4h4" />
    </svg>
  );
}

function EditMenuIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M4 20h4.5L19 9.5 14.5 5 4 15.5Z" />
      <path d="m13.5 6 4.5 4.5" />
    </svg>
  );
}

function LogsMenuIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M5 5h14" />
      <path d="M5 10h14" />
      <path d="M5 15h9" />
      <path d="M5 20h6" />
    </svg>
  );
}

function SettingsGearIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 24 24"
      fill="none"
    >
      <path
        fill="currentColor"
        fillRule="evenodd"
        d="M9.95 2.35h4.1l.52 2.42c.57.18 1.11.4 1.62.68l2.08-1.33 2.9 2.9-1.33 2.08c.28.51.5 1.05.68 1.62l2.43.52v4.1l-2.43.52c-.18.57-.4 1.11-.68 1.62l1.33 2.08-2.9 2.9-2.08-1.33c-.51.28-1.05.5-1.62.68l-.52 2.43h-4.1l-.52-2.43a8.55 8.55 0 0 1-1.62-.68l-2.08 1.33-2.9-2.9 1.33-2.08a8.55 8.55 0 0 1-.68-1.62l-2.43-.52v-4.1l2.43-.52c.18-.57.4-1.11.68-1.62L2.83 7.02l2.9-2.9 2.08 1.33c.51-.28 1.05-.5 1.62-.68l.52-2.42ZM12 16.95a3.65 3.65 0 1 0 0-7.3 3.65 3.65 0 0 0 0 7.3Z"
        clipRule="evenodd"
      />
    </svg>
  );
}

function AiSparkIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 24 24"
      fill="none"
    >
      <path
        fill="currentColor"
        d="M12 2.5 13.85 8.15 19.5 10 13.85 11.85 12 17.5 10.15 11.85 4.5 10 10.15 8.15 12 2.5ZM18 14l.9 2.6 2.6.9-2.6.9L18 21l-.9-2.6-2.6-.9 2.6-.9L18 14ZM6 14.5l.65 1.85L8.5 17l-1.85.65L6 19.5l-.65-1.85L3.5 17l1.85-.65L6 14.5Z"
      />
    </svg>
  );
}

function SnapFilterIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-4 w-4"
      viewBox="0 0 18 18"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.45"
      strokeLinecap="round"
      strokeLinejoin="round"
    >
      <path d="M9 2.5v3" />
      <path d="M9 12.5v3" />
      <path d="M2.5 9h3" />
      <path d="M12.5 9h3" />
      <circle cx="9" cy="9" r="2.7" />
      <path d="M3.8 3.8l2.1 2.1" />
      <path d="M12.1 12.1l2.1 2.1" />
    </svg>
  );
}

interface AppHeaderProps
  extends AppHeaderCreateToolbarProps,
    AppHeaderConstructToolbarProps {
  workspaceView: WorkspaceView;
  canOpenSlicerView: boolean;
  canExportToSlicer: boolean;
  onSetWorkspaceView: (view: WorkspaceView) => void;
  onExportToSlicer: () => void;
  status: string;
  disabled: boolean;
  canUndo: boolean;
  canRedo: boolean;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  selectedReferenceId: string | null;
  selectedFaceId: string | null;
  armedSketchConstraint: ArmedSketchConstraint;
  isMirrorToolOpen: boolean;
  // Arc tool's creation mode + setter — see SketchToolbar for the
  // segmented control's behaviour.
  arcToolMode: "three_point" | "center_start_end";
  onSetArcToolMode: (mode: "three_point" | "center_start_end") => void;
  // Rectangle tool's creation mode — split button variant.
  rectangleToolMode: "corner_corner" | "center_point" | "three_point";
  onSetRectangleToolMode: (mode: "corner_corner" | "center_point" | "three_point") => void;
  // Circle tool's creation mode — split button variant.
  circleToolMode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines";
  onSetCircleToolMode: (mode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines") => void;
  polygonToolMode: "circumscribed" | "inscribed" | "edge";
  onSetPolygonToolMode: (mode: "circumscribed" | "inscribed" | "edge") => void;
  onStart: () => Promise<void>;
  onCreateDocument: () => Promise<void>;
  onExportDocument: () => Promise<void>;
  onSaveDocument: () => Promise<void>;
  onLoadDocument: () => Promise<void>;
  onUndo: () => Promise<void>;
  onRedo: () => Promise<void>;
  logCount: number;
  errorLogCount: number;
  onOpenLogs: () => void;
  onOpenSettings: () => void;
  showAiAssistant: boolean;
  isAiPanelOpen: boolean;
  onToggleAiPanel: () => void;
  // Modify ribbon (Fillet / Chamfer). Enabled state is owned by the
  // parent so it can match the F-hotkey gating exactly.
  canEdgeOp: boolean;
  canMove: boolean;
  canShell: boolean;
  onFillet: () => Promise<void>;
  onChamfer: () => Promise<void>;
  onMove: () => Promise<void>;
  onShell: () => Promise<void>;
  onStartSketch: () => Promise<void>;
  onFinishSketch: () => Promise<void>;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onArmSketchConstraint: (constraint: ConstraintType) => Promise<void>;
  onStartMirrorTool: () => Promise<void>;
  onCancelSketchConstraint: () => void;
  onWorkspaceDropdownOpenChange?: (isOpen: boolean) => void;
  // Parameters panel
  parametersPanelOpen: boolean;
  onToggleParametersPanel: () => void;
  filterPanelOpen: boolean;
  onToggleFilterPanel: () => void;
  materialsPanelOpen: boolean;
  onToggleMaterialsPanel: () => void;
  onUpdateSelectionFilter: (filter: SelectionFilterUpdate) => Promise<void>;
  // CAM workspace
  activeCamOperation: CamOperationType | null;
  onSelectCamOperation: (op: CamOperationType) => void;
  hasCamSetup: boolean;
  onCamSetupClick: () => void;
  onCamFaceMillingClick: () => void;
}

export function AppHeader({
  workspaceView,
  canOpenSlicerView,
  canExportToSlicer,
  onSetWorkspaceView,
  onExportToSlicer,
  status,
  disabled,
  canUndo,
  canRedo,
  activeSketchPlaneId,
  activeSketchTool,
  selectedReferenceId,
  selectedFaceId,
  armedSketchConstraint,
  isMirrorToolOpen,
  arcToolMode,
  onSetArcToolMode,
  rectangleToolMode,
  onSetRectangleToolMode,
  circleToolMode,
  onSetCircleToolMode,
  polygonToolMode,
  onSetPolygonToolMode,
  onStart,
  onCreateDocument,
  onExportDocument,
  onSaveDocument,
  onLoadDocument,
  onUndo,
  onRedo,
  logCount,
  errorLogCount,
  onOpenLogs,
  onOpenSettings,
  showAiAssistant,
  isAiPanelOpen,
  onToggleAiPanel,
  onAddBoxFeature,
  onAddCylinderFeature,
  canExtrude,
  onExtrude,
  canLoft,
  onLoft,
  canRevolve,
  onRevolve,
  canSweep,
  onSweep,
  canHole,
  onHole,
  canThread,
  onThread,
  canFastener,
  onFastener,
  canEdgeOp,
  canMove,
  canShell,
  onFillet,
  onChamfer,
  onMove,
  onShell,
  canOffsetPlane,
  canMidplane,
  canTangentPlane,
  canAnglePlane,
  canConstructionAxis,
  canConstructionPoint,
  canHelix,
  onOffsetPlane,
  onMidplane,
  onTangentPlane,
  onAnglePlane,
  onConstructionAxis,
  onConstructionPoint,
  onHelix,
  onStartSketch,
  onFinishSketch,
  onSetSketchTool,
  onArmSketchConstraint,
  onStartMirrorTool,
  onCancelSketchConstraint,
  onWorkspaceDropdownOpenChange,
  parametersPanelOpen,
  onToggleParametersPanel,
  filterPanelOpen,
  onToggleFilterPanel,
  materialsPanelOpen,
  onToggleMaterialsPanel,
  onUpdateSelectionFilter,
  activeCamOperation,
  onSelectCamOperation,
  hasCamSetup,
  onCamSetupClick,
  onCamFaceMillingClick,
}: AppHeaderProps) {
  const { t: _t } = useTranslation();
  // Keep the main navigation bar in English regardless of the locale
  // setting, so non-native speakers can always read the menu items.
  const t = useCallback(
    (key: string, options?: Record<string, unknown>) =>
      key.startsWith("header.")
        ? _t(key, { ...options, lng: "en" })
        : _t(key, options),
    [_t],
  );
  const { config, updateConfig } = useAppConfig();
  const [activeCadWorkspace, setActiveCadWorkspace] =
    useState<(typeof workspaces)[number]>("create");
  const [activeCamWorkspace, setActiveCamWorkspace] =
    useState<CamWorkspace>("milling");
  const [openMenu, setOpenMenu] = useState<"box" | "cylinder" | null>(null);

  useEffect(() => {
    setOpenMenu(null);
  }, [activeCadWorkspace]);

  useEffect(() => {
    if (activeSketchPlaneId) {
      setActiveCadWorkspace("sketch");
    }
  }, [activeSketchPlaneId]);

  return (
    <header className="cad-ribbon relative z-20">
      <div className="flex items-center justify-between gap-5 px-5 py-1">
        <div className="flex items-center gap-6">
          <MenuDropdown
            align="start"
            label={
              workspaceView === "cad"
                ? t("workspace.cad")
                : workspaceView === "cam"
                  ? t("workspace.cam")
                  : t("workspace.slicer")
            }
            items={[
              {
                label: t("workspace.cad"),
                onSelect: () => onSetWorkspaceView("cad"),
              },
              {
                label: t("workspace.cam"),
                onSelect: () => onSetWorkspaceView("cam"),
              },
              {
                label: t("workspace.slicer"),
                disabled: !canOpenSlicerView,
                onSelect: () => onSetWorkspaceView("slicer"),
              },
            ]}
            onOpenChange={onWorkspaceDropdownOpenChange}
          />
          {workspaceView === "cad" ? (
            <nav className="flex items-center gap-1 rounded-full p-0.5 cad-subtle-block">
              {workspaces.map((workspace) => (
                <button
                  key={workspace}
                  className={
                    activeCadWorkspace === workspace
                      ? "cad-ribbon-tab cad-ribbon-tab-active"
                      : "cad-ribbon-tab"
                  }
                  onClick={() => {
                    setActiveCadWorkspace(workspace);
                  }}
                >
                  {t(`header.workspace.${workspace}`)}
                </button>
              ))}
            </nav>
          ) : null}
          {workspaceView === "cam" ? (
            <nav className="flex items-center gap-1 rounded-full p-0.5 cad-subtle-block">
              {camWorkspaces.map((workspace) => (
                <button
                  key={workspace}
                  className={
                    activeCamWorkspace === workspace
                      ? "cad-ribbon-tab cad-ribbon-tab-active"
                      : "cad-ribbon-tab"
                  }
                  onClick={() => {
                    setActiveCamWorkspace(workspace);
                  }}
                >
                  {t(`cam.category.${workspace}`)}
                </button>
              ))}
            </nav>
          ) : null}
          {workspaceView === "cad" ? (
            <div className="relative flex items-center gap-1.5">
              <button
                type="button"
                className={
                  parametersPanelOpen
                    ? "cad-ribbon-action cad-ribbon-action-primary h-9 px-3 leading-none"
                    : "cad-ribbon-action h-9 px-3 leading-none"
                }
                onClick={onToggleParametersPanel}
              >
                <span className="normal-case">f(x)</span>
              </button>
              <button
                type="button"
                className={
                  filterPanelOpen
                    ? "cad-ribbon-action cad-ribbon-action-primary h-9 gap-2 px-3 leading-none"
                    : "cad-ribbon-action h-9 gap-2 px-3 leading-none"
                }
                onClick={onToggleFilterPanel}
                title={t("selectionFilter.title")}
                aria-label={t("selectionFilter.title")}
              >
                <SnapFilterIcon />
                <span>{t("selectionFilter.button")}</span>
              </button>
              <button
                type="button"
                className={
                  materialsPanelOpen
                    ? "cad-ribbon-action cad-ribbon-action-primary h-9 px-3 leading-none"
                    : "cad-ribbon-action h-9 px-3 leading-none"
                }
                onClick={onToggleMaterialsPanel}
                title={t("materials.title")}
              >
                {t("materials.button")}
              </button>
              {filterPanelOpen ? (
                <div className="absolute left-0 top-[calc(100%+0.75rem)]">
                  <SelectionFilterPanel
                    currentFilter={readStoredFilter()}
                    open={filterPanelOpen}
                    onChange={(filter) => {
                      writeStoredFilter(filter);
                      void onUpdateSelectionFilter(filter);
                    }}
                    onClose={onToggleFilterPanel}
                  />
                </div>
              ) : null}
              {parametersPanelOpen ? (
                <div className="absolute left-0 top-[calc(100%+0.75rem)]">
                  <ParametersPanel onClose={onToggleParametersPanel} />
                </div>
              ) : null}
            </div>
          ) : null}
        </div>

        <div className="flex items-center gap-2">
          {status !== "connected" && status !== "starting" ? (
            // Hidden while the core is mid-launch so a double-click
            // can't kick off a second `start()` (which would race the
            // first one's status flip and confuse the auto-doc effect
            // in App.tsx).
            <button
              className="cad-ribbon-action cad-ribbon-action-primary"
              onClick={() => void onStart()}
            >
              {t("header.startCore")}
            </button>
          ) : null}
          <MenuDropdown
            label={t("header.file")}
            icon={<FileMenuIcon />}
            disabled={disabled}
            items={[
              { label: t("header.new"), onSelect: () => void onCreateDocument() },
              { label: t("header.open"), onSelect: () => void onLoadDocument() },
              { label: t("header.save"), onSelect: () => void onSaveDocument() },
              {
                label: t("header.exportStep"),
                onSelect: () => void onExportDocument(),
              },
            ]}
          />
          <MenuDropdown
            label={t("header.edit")}
            icon={<EditMenuIcon />}
            disabled={disabled}
            items={[
              {
                label: t("header.undo"),
                disabled: !canUndo,
                onSelect: () => void onUndo(),
              },
              {
                label: t("header.redo"),
                disabled: !canRedo,
                onSelect: () => void onRedo(),
              },
            ]}
          />
          <button
            type="button"
            className="cad-ribbon-action"
            onClick={onOpenLogs}
            aria-label={t("header.logs")}
            title={t("header.logs")}
          >
            <LogsMenuIcon />
            <span
              className={`ml-1.5 rounded-full px-1.5 py-0.5 text-[0.65rem] ${
                errorLogCount > 0
                  ? "bg-danger/20 text-danger"
                  : "bg-white/10 text-on-surface-dim"
              }`}
            >
              {errorLogCount > 0 ? errorLogCount : logCount}
            </span>
          </button>
          <button
            type="button"
            className="cad-ribbon-action h-8 w-10 px-0 py-0 text-[11px] font-semibold uppercase tracking-wider text-on-surface-muted hover:text-on-surface"
            onClick={() =>
              updateConfig((prev) => ({
                ...prev,
                displayUnits:
                  prev.displayUnits === "mm" ? "in" : "mm",
              }))
            }
            aria-label={t("settings.toggleUnits")}
            title={t("settings.toggleUnits", {
              units: config.displayUnits.toUpperCase(),
            })}
          >
            {config.displayUnits}
          </button>
          {showAiAssistant ? (
            <button
              type="button"
              className={
                isAiPanelOpen
                  ? "cad-ribbon-action h-8 w-8 px-0 py-0 text-primary-glow"
                  : "cad-ribbon-action h-8 w-8 px-0 py-0 text-on-surface-muted hover:text-on-surface"
              }
              onClick={onToggleAiPanel}
              aria-label={t("header.aiAssistant")}
              title={t("header.aiAssistant")}
            >
              <AiSparkIcon />
            </button>
          ) : null}
          {/*
          <div className="cad-status-pill">
            <span
              className={`h-2.5 w-2.5 rounded-full ${
                status === "connected"
                  ? "bg-success cad-status-dot-online"
                  : "bg-danger cad-status-dot-offline"
              }`}
            />
            <span>
              {status === "connected"
                ? t("header.localSession")
                : t("header.coreOffline")}
            </span>
          </div>
          */}
          <button
            type="button"
            className="cad-ribbon-action h-8 w-8 px-0 py-0 text-on-surface-muted hover:text-on-surface"
            onClick={onOpenSettings}
            aria-label={t("header.settings")}
            title={t("header.settings")}
          >
            <SettingsGearIcon />
          </button>
        </div>
      </div>

      {workspaceView === "cad" ? (
        <div
          className="flex items-center justify-between gap-3 px-4 py-1"
          style={{ borderTop: "1px solid var(--cad-panel-soft-border)" }}
        >
          <div className="flex min-w-0 items-center gap-3">
            {activeCadWorkspace === "create" ? (
              <CreateToolbar
                openMenu={openMenu}
                disabled={disabled}
                setOpenMenu={setOpenMenu}
                onAddBoxFeature={onAddBoxFeature}
                onAddCylinderFeature={onAddCylinderFeature}
                canExtrude={canExtrude}
                onExtrude={onExtrude}
                canLoft={canLoft}
                onLoft={onLoft}
                canRevolve={canRevolve}
                onRevolve={onRevolve}
                canSweep={canSweep}
                onSweep={onSweep}
                canHole={canHole}
                onHole={onHole}
                canThread={canThread}
                onThread={onThread}
                canFastener={canFastener}
                onFastener={onFastener}
              />
            ) : null}

            {activeCadWorkspace === "modify" ? (
              <ModifyToolbar
                disabled={disabled}
                canEdgeOp={canEdgeOp}
                canMove={canMove}
                canShell={canShell}
                onFillet={() => void onFillet()}
                onChamfer={() => void onChamfer()}
                onMove={() => void onMove()}
                onShell={() => void onShell()}
              />
            ) : null}

            {activeCadWorkspace === "construct" ? (
              <ConstructToolbar
                disabled={disabled}
                canOffsetPlane={canOffsetPlane}
                canMidplane={canMidplane}
                canTangentPlane={canTangentPlane}
                canAnglePlane={canAnglePlane}
                canConstructionAxis={canConstructionAxis}
                canConstructionPoint={canConstructionPoint}
                canHelix={canHelix}
                onOffsetPlane={onOffsetPlane}
                onMidplane={onMidplane}
                onTangentPlane={onTangentPlane}
                onAnglePlane={onAnglePlane}
                onConstructionAxis={onConstructionAxis}
                onConstructionPoint={onConstructionPoint}
                onHelix={onHelix}
              />
            ) : null}

            {activeCadWorkspace === "sketch" ? (
              <SketchToolbar
                activeSketchPlaneId={activeSketchPlaneId}
                activeSketchTool={activeSketchTool}
                selectedReferenceId={selectedReferenceId}
                selectedFaceId={selectedFaceId}
                armedSketchConstraint={armedSketchConstraint}
                isMirrorToolOpen={isMirrorToolOpen}
                arcToolMode={arcToolMode}
                onSetArcToolMode={onSetArcToolMode}
                rectangleToolMode={rectangleToolMode}
                onSetRectangleToolMode={onSetRectangleToolMode}
                circleToolMode={circleToolMode}
                onSetCircleToolMode={onSetCircleToolMode}
                polygonToolMode={polygonToolMode}
                onSetPolygonToolMode={onSetPolygonToolMode}
                onStartSketch={onStartSketch}
                onFinishSketch={onFinishSketch}
                onCancelSketchConstraint={onCancelSketchConstraint}
                onSetSketchTool={onSetSketchTool}
                onArmSketchConstraint={onArmSketchConstraint}
                onStartMirrorTool={onStartMirrorTool}
              />
            ) : null}
            {activeCadWorkspace === "sketch" ? <SketchDofBadge /> : null}
          </div>

          {canExportToSlicer ? (
            <button
              type="button"
              className="cad-ribbon-action"
              disabled={disabled}
              onClick={onExportToSlicer}
            >
              {t("workspace.exportToSlicer")}
            </button>
          ) : null}
        </div>
      ) : workspaceView === "cam" ? (
        <div
          className="flex items-center justify-between gap-3 px-4 py-1"
          style={{ borderTop: "1px solid var(--cad-panel-soft-border)" }}
        >
          {activeCamWorkspace === "milling" ? (
            <CamMillingToolbar
              disabled={disabled}
              hasSetup={hasCamSetup}
              onSetupClick={onCamSetupClick}
              onFaceMillingClick={onCamFaceMillingClick}
            />
          ) : activeCamWorkspace === "turning" ? (
            <CamTurningToolbar disabled={disabled} />
          ) : activeCamWorkspace === "printing" ? (
            <CamPrintingToolbar disabled={disabled} />
          ) : (
            <CamCuttingToolbar disabled={disabled} />
          )}
        </div>
      ) : null}
    </header>
  );
}
