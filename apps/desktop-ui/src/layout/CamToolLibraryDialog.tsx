// Tool library manager — a modal with two sources and three panes:
//   · left nav: document library / shared on-disk library + type filter
//   · middle: searchable, sortable tool list (double-click = edit)
//   · right: detail pane with the schematic + per-source actions
// (new / duplicate / delete / copy-to-document / save-to-shared).
// Editing opens the floating CamToolEditorPanel in place.
//
// The dialog calls useCadCore() directly for the tool commands, so
// the only props it needs from its mount point are the document, the
// runAction gate and message sink.

import { useEffect, useMemo, useState } from "react";
import { useTranslation } from "react-i18next";

import { ScrollArea } from "@/lib";
import { useCadCore } from "@/hooks/useCadCore";
import type { ToolEntry, ToolType } from "@/types";
import type { DocumentState } from "@/types/ipc";
import { computedFeedMmPerMin, computedSpindleRpm } from "@/lib/toolOptions";
import { CamToolEditorPanel } from "./CamToolEditorPanel";
import { ToolSchematicView } from "./ToolSchematicView";
import { useCamEscapeCancel } from "./camPanelShared";

const TOOL_TYPES: ToolType[] = [
  "endmill_flat",
  "endmill_ball",
  "endmill_bull",
  "facemill",
  "drill",
  "spot_drill",
  "chamfer",
  "v_bit",
  "threadmill",
  "turning_insert",
  "laser",
  "plasma",
];

type LibrarySource = "document" | "shared";

/** Blank template mirroring the core ToolEntry defaults in cam_types.h. */
function blankTool(): ToolEntry {
  return {
    tool_id: "",
    name: "",
    type: "endmill_flat",
    tool_number: 0,
    pocket_number: 0,
    description: "",
    vendor: "",
    product_id: "",
    guid: "",
    diameter_mm: 6,
    corner_radius_mm: 0,
    flute_length_mm: 20,
    overall_length_mm: 60,
    shank_diameter_mm: 6,
    shoulder_length_mm: 0,
    length_below_holder_mm: 0,
    flutes: 2,
    helix_angle_deg: 30,
    point_angle_deg: 118,
    tip_diameter_mm: 0,
    tip_length_mm: 0,
    taper_angle_deg: 0,
    front_angle_deg: 0,
    back_angle_deg: 0,
    orientation: 0,
    material: "carbide",
    coolant_through: false,
    max_spindle_rpm: 15000,
    surface_speed_m_per_min: 100,
    feed_per_tooth_mm: 0.05,
    default_feedrate_mm_per_min: 1000,
    default_plunge_feedrate_mm_per_min: 500,
    default_stepdown_mm: 1,
    default_stepover_percent: 50,
  };
}

interface CamToolLibraryDialogProps {
  document: DocumentState | null;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
  onClose: () => void;
}

export function CamToolLibraryDialog({
  document,
  runAction,
  addMessage,
  onClose,
}: CamToolLibraryDialogProps) {
  const { t } = useTranslation();
  const {
    camToolAdd,
    camToolUpdate,
    camToolDelete,
    camToolLibraryList,
    camToolLibrarySave,
  } = useCadCore();

  const [source, setSource] = useState<LibrarySource>("document");
  const [typeFilter, setTypeFilter] = useState<ToolType | "all">("all");
  const [query, setQuery] = useState("");
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [shared, setShared] = useState<ToolEntry[]>([]);
  const [confirmDelete, setConfirmDelete] = useState(false);
  const [editor, setEditor] = useState<{
    tool: ToolEntry;
    mode: "create" | "edit";
  } | null>(null);

  // Escape closes the dialog — unless the editor is showing, where
  // Escape only returns to the library list (the editor installs its
  // own handler while mounted).
  useCamEscapeCancel(() => {
    if (!editor) {
      onClose();
    }
  });

  // Load the shared on-disk library once (and refresh after saves).
  const refreshShared = async () => {
    const tools = await camToolLibraryList();
    setShared(tools);
  };
  useEffect(() => {
    void refreshShared();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const tools = useMemo(() => {
    const all = source === "document"
      ? (document?.cam.tool_library ?? [])
      : shared;
    const lowered = query.trim().toLowerCase();
    return all
      .filter((tool) => typeFilter === "all" || tool.type === typeFilter)
      .filter(
        (tool) =>
          lowered === "" ||
          tool.name.toLowerCase().includes(lowered) ||
          tool.vendor.toLowerCase().includes(lowered) ||
          tool.product_id.toLowerCase().includes(lowered),
      )
      .sort((a, b) => {
        // Numbered tools first (ascending), then by name.
        if (a.tool_number !== b.tool_number) {
          if (a.tool_number === 0) return 1;
          if (b.tool_number === 0) return -1;
          return a.tool_number - b.tool_number;
        }
        return a.name.localeCompare(b.name);
      });
  }, [document, query, shared, source, typeFilter]);

  const selected =
    tools.find((tool) => tool.tool_id === selectedId) ?? tools[0] ?? null;

  const referencingOps = useMemo(() => {
    if (!selected) return [];
    return (document?.cam.operations ?? []).filter(
      (op) => op.tool_id === selected.tool_id,
    );
  }, [document, selected]);

  const commitTool = async (tool: ToolEntry, mode: "create" | "edit") => {
    await runAction(async () => {
      if (source === "shared") {
        await camToolLibrarySave(tool);
        await refreshShared();
        addMessage(t("cam.toolLibrary.savedToShared", "Tool saved to the shared library."));
      } else if (mode === "create") {
        await camToolAdd(tool);
      } else {
        await camToolUpdate(tool.tool_id, tool);
      }
    });
    setEditor(null);
  };

  const duplicateSelected = async () => {
    if (!selected) return;
    await runAction(async () => {
      await camToolAdd({
        ...selected,
        tool_id: "",
        guid: "",
        tool_number: 0,
        pocket_number: 0,
        name: `${selected.name} (copy)`,
      });
    });
    setConfirmDelete(false);
  };

  const deleteSelected = async () => {
    if (!selected) return;
    if (referencingOps.length > 0 && !confirmDelete) {
      setConfirmDelete(true);
      return;
    }
    await runAction(async () => {
      await camToolDelete(selected.tool_id);
    });
    setSelectedId(null);
    setConfirmDelete(false);
  };

  const copySelectedToDocument = async () => {
    if (!selected) return;
    await runAction(async () => {
      // guid cleared so the core mints a fresh identity; tool_number 0
      // so the core auto-assigns the next free document number.
      await camToolAdd({ ...selected, tool_id: "", guid: "", tool_number: 0, pocket_number: 0 });
    });
    setSource("document");
  };

  const saveSelectedToShared = async () => {
    if (!selected) return;
    await runAction(async () => {
      await camToolLibrarySave(selected);
      await refreshShared();
    });
    addMessage(t("cam.toolLibrary.savedToShared", "Tool saved to the shared library."));
  };

  // Editing replaces the dialog content; Escape in the editor returns
  // to the library view, Escape in the library closes the dialog.
  if (editor) {
    return (
      <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/55 px-6 py-8 backdrop-blur-sm">
        <CamToolEditorPanel
          tool={editor.tool}
          mode={editor.mode}
          library={document?.cam.tool_library ?? []}
          onSave={(tool) => {
            void commitTool(tool, editor.mode);
          }}
          onClose={() => setEditor(null)}
        />
      </div>
    );
  }

  const typeOptions: Array<{ value: string; label: string }> = [
    { value: "all", label: t("cam.toolLibrary.allTypes", "All types") },
    ...TOOL_TYPES.map((type) => ({
      value: type,
      label: t(`cam.toolEditor.types.${type}`, type),
    })),
  ];

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/55 px-6 py-8 backdrop-blur-sm"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) {
          onClose();
        }
      }}
    >
      <div className="cad-floating-panel flex max-h-full min-h-0 w-[900px] max-w-full flex-col overflow-hidden px-5 py-5">
        {/* ── Header ─────────────────────────────────────────────── */}
        <div className="flex items-center justify-between">
          <p className="cad-kicker">
            {t("cam.toolLibrary.title", "Tool Library")}
          </p>
          <button
            type="button"
            className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider hover:opacity-80"
            onClick={onClose}
          >
            {t("cam.toolLibrary.close", "Close")}
          </button>
        </div>

        <div className="mt-4 flex min-h-0 flex-1 gap-4 overflow-hidden">
          {/* ── Nav ──────────────────────────────────────────────── */}
          <nav className="flex w-[150px] shrink-0 flex-col gap-1">
            <button
              type="button"
              className={
                source === "document"
                  ? "cad-tool-button-active rounded-lg px-3 py-2 text-left text-xs"
                  : "cad-tool-button rounded-lg px-3 py-2 text-left text-xs"
              }
              onClick={() => {
                setSource("document");
                setSelectedId(null);
                setConfirmDelete(false);
              }}
            >
              {t("cam.toolLibrary.documentLibrary", "Document Library")}
            </button>
            <button
              type="button"
              className={
                source === "shared"
                  ? "cad-tool-button-active rounded-lg px-3 py-2 text-left text-xs"
                  : "cad-tool-button rounded-lg px-3 py-2 text-left text-xs"
              }
              onClick={() => {
                setSource("shared");
                setSelectedId(null);
                setConfirmDelete(false);
              }}
            >
              {t("cam.toolLibrary.sharedLibrary", "Shared Library")}
            </button>
            <p className="mt-2 text-[10px] uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.toolLibrary.filterByType", "Filter by type")}
            </p>
            <select
              className="cad-input mt-1"
              value={typeFilter}
              onChange={(event) =>
                setTypeFilter(event.currentTarget.value as ToolType | "all")
              }
            >
              {typeOptions.map((option) => (
                <option key={option.value} value={option.value}>
                  {option.label}
                </option>
              ))}
            </select>
            <p className="mt-3 text-[10px] leading-relaxed text-on-surface-dim">
              {t(
                "cam.toolLibrary.note",
                "The shared library lives on disk and is available to every document.",
              )}
            </p>
          </nav>

          {/* ── List ─────────────────────────────────────────────── */}
          <div className="flex min-h-0 w-[280px] shrink-0 flex-col">
            <input
              className="cad-input"
              type="text"
              placeholder={t("cam.toolLibrary.search", "Search tools…")}
              value={query}
              onChange={(event) => setQuery(event.currentTarget.value)}
            />
            <ScrollArea className="mt-2 min-h-0 flex-1" viewportClassName="space-y-0.5">
              {tools.length === 0 ? (
                <p className="px-2 py-4 text-center text-xs text-on-surface-dim">
                  {t("cam.toolLibrary.empty", "No tools match.")}
                </p>
              ) : (
                tools.map((tool) => (
                  <button
                    key={tool.tool_id}
                    type="button"
                    className={
                      selected?.tool_id === tool.tool_id
                        ? "cad-panel-item-active flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-xs"
                        : "cad-panel-item flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-xs"
                    }
                    onClick={() => {
                      setSelectedId(tool.tool_id);
                      setConfirmDelete(false);
                    }}
                    onDoubleClick={() =>
                      setEditor({ tool, mode: "edit" })
                    }
                  >
                    <span className="min-w-0 flex-1 truncate">
                      <span className="text-on-surface-muted">
                        {tool.tool_number > 0 ? `T${tool.tool_number} ` : ""}
                      </span>
                      {tool.name}
                    </span>
                    <span className="ml-2 shrink-0 text-[10px] text-on-surface-dim">
                      {t(`cam.toolEditor.types.${tool.type}`, tool.type)}
                    </span>
                  </button>
                ))
              )}
            </ScrollArea>
            <button
              type="button"
              className="cad-action-primary mt-2"
              onClick={() =>
                setEditor({ tool: blankTool(), mode: "create" })
              }
            >
              {t("cam.toolLibrary.newTool", "New Tool")}
            </button>
          </div>

          {/* ── Detail ───────────────────────────────────────────── */}
          <div className="cad-panel-soft flex min-h-0 min-w-0 flex-1 flex-col rounded-xl p-3">
            {selected ? (
              <>
                <div className="flex min-h-0 gap-3">
                  <ToolSchematicView
                    tool={selected}
                    className="h-[260px] w-[220px] shrink-0"
                  />
                  <div className="min-w-0 flex-1 space-y-1 text-xs">
                    <p className="font-semibold">
                      {selected.tool_number > 0 ? `T${selected.tool_number} · ` : ""}
                      {selected.name}
                    </p>
                    <p className="text-on-surface-muted">
                      {t(`cam.toolEditor.types.${selected.type}`, selected.type)}
                    </p>
                    {selected.vendor || selected.product_id ? (
                      <p className="text-on-surface-dim">
                        {[selected.vendor, selected.product_id]
                          .filter(Boolean)
                          .join(" · ")}
                      </p>
                    ) : null}
                    {selected.description ? (
                      <p className="text-on-surface-dim">{selected.description}</p>
                    ) : null}
                    <p className="pt-1 text-on-surface-dim">
                      {t("cam.toolLibrary.diameter", "Ø")} {selected.diameter_mm} mm
                      {" · "}
                      {t("cam.toolLibrary.fluteLengthShort", "FL")} {selected.flute_length_mm} mm
                      {" · "}
                      {t("cam.toolLibrary.overallLengthShort", "OAL")} {selected.overall_length_mm} mm
                    </p>
                    <p className="text-on-surface-dim">
                      {t("cam.toolLibrary.surfaceSpeedShort", "Vc")}{" "}
                      {selected.surface_speed_m_per_min} m/min
                      {" · "}
                      {t("cam.toolLibrary.feedPerToothShort", "fz")}{" "}
                      {selected.feed_per_tooth_mm} mm
                    </p>
                    <p className="text-on-surface-dim">
                      {computedSpindleRpm(selected) > 0
                        ? `${computedSpindleRpm(selected)} RPM`
                        : "—"}
                      {" · "}
                      {computedFeedMmPerMin(selected) > 0
                        ? `${computedFeedMmPerMin(selected)} mm/min`
                        : "—"}
                    </p>
                    {referencingOps.length > 0 ? (
                      <p className="pt-1 text-on-surface-muted">
                        {t("cam.toolLibrary.referencedBy", {
                          count: referencingOps.length,
                        })}
                      </p>
                    ) : null}
                  </div>
                </div>
                <div className="mt-auto flex flex-wrap gap-2 pt-3">
                  <button
                    type="button"
                    className="cad-action-ghost"
                    onClick={() => setEditor({ tool: selected, mode: "edit" })}
                  >
                    {t("cam.toolLibrary.edit", "Edit")}
                  </button>
                  {source === "document" ? (
                    <>
                      <button
                        type="button"
                        className="cad-action-ghost"
                        onClick={() => {
                          void duplicateSelected();
                        }}
                      >
                        {t("cam.toolLibrary.duplicate", "Duplicate")}
                      </button>
                      <button
                        type="button"
                        className={
                          referencingOps.length > 0 && !confirmDelete
                            ? "cad-action-ghost text-warning"
                            : "cad-action-ghost text-danger"
                        }
                        onClick={() => {
                          void deleteSelected();
                        }}
                      >
                        {confirmDelete
                          ? t("cam.toolLibrary.confirmDelete", "Delete?")
                          : t("cam.toolLibrary.delete", "Delete")}
                      </button>
                      <button
                        type="button"
                        className="cad-action-ghost"
                        onClick={() => {
                          void saveSelectedToShared();
                        }}
                      >
                        {t("cam.toolLibrary.saveToShared", "Save to Shared")}
                      </button>
                    </>
                  ) : (
                    <button
                      type="button"
                      className="cad-action-ghost"
                      onClick={() => {
                        void copySelectedToDocument();
                      }}
                    >
                      {t("cam.toolLibrary.copyToDocument", "Copy to Document")}
                    </button>
                  )}
                </div>
              </>
            ) : (
              <p className="px-2 py-4 text-center text-xs text-on-surface-dim">
                {t("cam.toolLibrary.selectTool", "Select a tool to see its details.")}
              </p>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
