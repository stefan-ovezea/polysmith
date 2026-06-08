import { useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import type {
  DragEvent,
  FormEvent,
  KeyboardEvent,
  MouseEvent,
  PointerEvent as ReactPointerEvent,
} from "react";
import { useTranslation } from "react-i18next";
import { Checkbox } from "@/lib";
import type { ProjectFolder, RecentProject, RecentProjectsDocument } from "@/lib";
import { ContextMenuShell } from "./ContextMenuShell";
import { buildProjectBreadcrumbs } from "./projectBreadcrumbs";

interface ProjectsPanelProps {
  document: RecentProjectsDocument;
  activeProjectPath: string | null;
  onOpenProject: (project: RecentProject) => void;
  onCreateFolder: (name: string, parentFolderId: string | null) => void;
  onMoveProject: (projectPath: string, folderId: string | null) => void;
  onDeleteProject: (project: RecentProject, shouldDeleteFile: boolean) => void;
  onDeleteFolder: (folderId: string) => void;
  onRenameProject: (project: RecentProject, name: string) => void;
  onRenameFolder: (folderId: string, name: string) => void;
  onCreateProject: (parentFolderId: string | null) => void;
}

interface DeleteProjectRequest {
  project: RecentProject;
  shouldDeleteFile: boolean;
}

interface MoveProjectRequest {
  project: RecentProject;
  folderId: string | null;
}

type ContextMenuTarget =
  | { kind: "project"; project: RecentProject }
  | { kind: "folder"; folder: ProjectFolder };

interface ProjectContextMenu {
  x: number;
  y: number;
  target: ContextMenuTarget;
}

type RenameTarget =
  | { kind: "project"; project: RecentProject; name: string }
  | { kind: "folder"; folder: ProjectFolder; name: string };

type DropTarget =
  | { kind: "level"; folderId: string | null }
  | { kind: "breadcrumb"; folderId: string | null }
  | { kind: "folder"; folderId: string };

interface PointerProjectDrag {
  projectPath: string;
  startX: number;
  startY: number;
  isDragging: boolean;
}

function ProjectPlaceholder() {
  return (
    <div className="cad-project-thumbnail-placeholder" aria-hidden="true">
      <svg
        viewBox="0 0 24 24"
        width="24"
        height="24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.5"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        <path d="M12 3 20 7v8l-8 4-8-4V7Z" />
        <path d="m4 7 8 4 8-4" />
        <path d="M12 11v8" />
      </svg>
    </div>
  );
}

function FolderIcon({ size = 14 }: { size?: number }) {
  return (
    <svg
      viewBox="0 0 24 24"
      width={size}
      height={size}
      fill="none"
      stroke="currentColor"
      strokeWidth="1.7"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M3 6.5h6l2 2h10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2Z" />
      <path d="M3 6.5v-1a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v1" />
    </svg>
  );
}

function HomeIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="14"
      height="14"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="m3 10 9-7 9 7" />
      <path d="M5 9v11h14V9" />
      <path d="M10 20v-6h4v6" />
    </svg>
  );
}

function DeleteIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="14"
      height="14"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      aria-hidden="true"
    >
      <path d="M6 6l12 12" />
      <path d="M18 6 6 18" />
    </svg>
  );
}

function PlusIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="14"
      height="14"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      aria-hidden="true"
    >
      <path d="M12 5v14" />
      <path d="M5 12h14" />
    </svg>
  );
}

function ProjectIcon() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="14"
      height="14"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.7"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M5 3h10l4 4v14H5Z" />
      <path d="M15 3v5h5" />
    </svg>
  );
}

function ProjectCard({
  project,
  isActive,
  onOpen,
  onDelete,
  onContextMenu,
  onPointerDragStart,
  onDragStart,
  onDragEnd,
}: {
  project: RecentProject;
  isActive: boolean;
  onOpen: (project: RecentProject) => void;
  onDelete: (project: RecentProject) => void;
  onContextMenu: (project: RecentProject, event: MouseEvent) => void;
  onPointerDragStart: (
    projectPath: string,
    event: ReactPointerEvent<HTMLDivElement>,
  ) => void;
  onDragStart: (projectPath: string) => void;
  onDragEnd: () => void;
}) {
  const { t } = useTranslation();
  return (
    <div
      className={
        isActive ? "cad-project-card cad-project-card-active" : "cad-project-card"
      }
      draggable={false}
      onDragStart={(event) => {
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("application/x-polysmith-project", project.path);
        event.dataTransfer.setData("text/plain", project.path);
        onDragStart(project.path);
      }}
      onDragEnd={onDragEnd}
      onPointerDown={(event) => onPointerDragStart(project.path, event)}
      onContextMenu={(event) => onContextMenu(project, event)}
      title={project.path}
    >
      <div
        role="button"
        tabIndex={0}
        className="cad-project-open-button"
        onClick={() => onOpen(project)}
        onKeyDown={(event: KeyboardEvent<HTMLDivElement>) => {
          if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            onOpen(project);
          }
        }}
      >
        {project.thumbnailDataUrl ? (
          <img
            src={project.thumbnailDataUrl}
            alt=""
            className="cad-project-thumbnail"
            draggable={false}
          />
        ) : (
          <ProjectPlaceholder />
        )}
        <span className="min-w-0 flex-1 truncate text-left text-sm text-on-surface">
          {project.name}
        </span>
      </div>
      <button
        type="button"
        className="cad-project-delete-button"
        onClick={() => onDelete(project)}
        aria-label={t("projects.deleteProject", { name: project.name })}
        title={t("projects.deleteProject", { name: project.name })}
      >
        <DeleteIcon />
      </button>
    </div>
  );
}

function FolderCard({
  folder,
  isDropTarget,
  canAcceptDrop,
  onOpen,
  onDelete,
  onContextMenu,
  onDragTarget,
  onDropProject,
}: {
  folder: ProjectFolder;
  isDropTarget: boolean;
  canAcceptDrop: boolean;
  onOpen: (folderId: string) => void;
  onDelete: (folderId: string) => void;
  onContextMenu: (folder: ProjectFolder, event: MouseEvent) => void;
  onDragTarget: (folderId: string) => void;
  onDropProject: (folderId: string, event: DragEvent) => void;
}) {
  const { t } = useTranslation();
  return (
    <div
      className={
        isDropTarget
          ? "cad-project-folder-card cad-project-folder-card-drop-target"
          : "cad-project-folder-card"
      }
      data-project-folder-id={folder.id}
      onDragOverCapture={(event) => {
        if (!canAcceptDrop) {
          return;
        }
        event.stopPropagation();
        event.preventDefault();
        event.dataTransfer.dropEffect = "move";
        onDragTarget(folder.id);
      }}
      onDragEnterCapture={(event) => {
        if (!canAcceptDrop) {
          return;
        }
        event.stopPropagation();
        onDragTarget(folder.id);
      }}
      onDropCapture={(event) => {
        if (!canAcceptDrop) {
          return;
        }
        onDropProject(folder.id, event);
      }}
      onContextMenu={(event) => onContextMenu(folder, event)}
    >
      <button
        type="button"
        className="cad-project-folder-open-button"
        onClick={() => onOpen(folder.id)}
      >
        <FolderIcon size={20} />
        <span className="min-w-0 flex-1 truncate text-left text-sm text-on-surface">
          {folder.name}
        </span>
      </button>
      <button
        type="button"
        className="cad-project-delete-button"
        onClick={() => onDelete(folder.id)}
        aria-label={t("projects.deleteFolder", { name: folder.name })}
        title={t("projects.deleteFolder", { name: folder.name })}
      >
        <DeleteIcon />
      </button>
    </div>
  );
}

export function ProjectsPanel({
  document,
  activeProjectPath,
  onOpenProject,
  onCreateFolder,
  onMoveProject,
  onDeleteProject,
  onDeleteFolder,
  onRenameProject,
  onRenameFolder,
  onCreateProject,
}: ProjectsPanelProps) {
  const { t } = useTranslation();
  const [currentFolderId, setCurrentFolderId] = useState<string | null>(null);
  const [isCreatingFolder, setIsCreatingFolder] = useState(false);
  const [isNewMenuOpen, setIsNewMenuOpen] = useState(false);
  const [folderName, setFolderName] = useState("");
  const [draggedProjectPath, setDraggedProjectPath] = useState<string | null>(
    null,
  );
  const draggedProjectPathRef = useRef<string | null>(null);
  const [dropTarget, setDropTarget] = useState<DropTarget | null>(null);
  const [pointerProjectDrag, setPointerProjectDrag] =
    useState<PointerProjectDrag | null>(null);
  const pointerProjectDragRef = useRef<PointerProjectDrag | null>(null);
  const suppressProjectOpenRef = useRef(false);
  const [deleteProjectRequest, setDeleteProjectRequest] =
    useState<DeleteProjectRequest | null>(null);
  const [moveProjectRequest, setMoveProjectRequest] =
    useState<MoveProjectRequest | null>(null);
  const [deleteFolderRequest, setDeleteFolderRequest] =
    useState<ProjectFolder | null>(null);
  const [contextMenu, setContextMenu] = useState<ProjectContextMenu | null>(null);
  const [renameTarget, setRenameTarget] = useState<RenameTarget | null>(null);

  const foldersById = useMemo(
    () => new Map(document.folders.map((folder) => [folder.id, folder])),
    [document.folders],
  );
  const projectsByPath = useMemo(
    () => new Map(document.projects.map((project) => [project.path, project])),
    [document.projects],
  );
  const parentByFolderId = useMemo(() => {
    const next = new Map<string, string | null>();
    for (const folderId of document.rootFolderIds) {
      next.set(folderId, null);
    }
    for (const folder of document.folders) {
      for (const childId of folder.childFolderIds) {
        next.set(childId, folder.id);
      }
    }
    return next;
  }, [document.folders, document.rootFolderIds]);

  useEffect(() => {
    if (currentFolderId !== null && !foldersById.has(currentFolderId)) {
      setCurrentFolderId(null);
    }
  }, [currentFolderId, foldersById]);

  useEffect(() => {
    if (!contextMenu && !isNewMenuOpen) {
      return;
    }
    function closeFloatingMenus() {
      setContextMenu(null);
      setIsNewMenuOpen(false);
    }
    window.addEventListener("click", closeFloatingMenus);
    window.addEventListener("blur", closeFloatingMenus);
    return () => {
      window.removeEventListener("click", closeFloatingMenus);
      window.removeEventListener("blur", closeFloatingMenus);
    };
  }, [contextMenu, isNewMenuOpen]);

  useEffect(() => {
    if (!pointerProjectDrag) {
      return;
    }

    function readBreadcrumbFolderId(element: Element | null) {
      const rawFolderId = element
        ?.closest<HTMLElement>("[data-project-breadcrumb-folder-id]")
        ?.dataset.projectBreadcrumbFolderId;
      if (rawFolderId === undefined) {
        return undefined;
      }
      return rawFolderId === "root" ? null : rawFolderId;
    }

    function readDropTargetAt(clientX: number, clientY: number): DropTarget {
      const element = window.document.elementFromPoint(clientX, clientY);
      const breadcrumbFolderId = readBreadcrumbFolderId(element);
      if (breadcrumbFolderId !== undefined) {
        return { kind: "breadcrumb", folderId: breadcrumbFolderId };
      }
      const folderId =
        element
          ?.closest<HTMLElement>("[data-project-folder-id]")
          ?.dataset.projectFolderId ?? null;
      return folderId
        ? { kind: "folder", folderId }
        : { kind: "level", folderId: currentFolderId };
    }

    function handlePointerMove(event: PointerEvent) {
      const currentDrag = pointerProjectDragRef.current ?? pointerProjectDrag;
      if (!currentDrag) {
        return;
      }
      const distance = Math.hypot(
        event.clientX - currentDrag.startX,
        event.clientY - currentDrag.startY,
      );
      if (distance < 4 && !currentDrag.isDragging) {
        return;
      }
      if (!currentDrag.isDragging) {
        const nextDrag = { ...currentDrag, isDragging: true };
        pointerProjectDragRef.current = nextDrag;
        draggedProjectPathRef.current = currentDrag.projectPath;
        setDraggedProjectPath(currentDrag.projectPath);
        setPointerProjectDrag(nextDrag);
      }
      setDropTarget(readDropTargetAt(event.clientX, event.clientY));
    }

    function handlePointerUp(event: PointerEvent) {
      const currentDrag = pointerProjectDragRef.current ?? pointerProjectDrag;
      if (!currentDrag) {
        return;
      }
      const nextDropTarget = readDropTargetAt(event.clientX, event.clientY);
      const shouldMove =
        currentDrag.isDragging && nextDropTarget.kind !== "level";
      if (currentDrag.isDragging) {
        suppressProjectOpenRef.current = true;
      }
      const projectPath = currentDrag.projectPath;
      pointerProjectDragRef.current = null;
      setPointerProjectDrag(null);
      clearProjectDrag();
      if (shouldMove) {
        onMoveProject(projectPath, nextDropTarget.folderId);
      }
    }

    window.addEventListener("pointermove", handlePointerMove);
    window.addEventListener("pointerup", handlePointerUp);
    window.addEventListener("pointercancel", handlePointerUp);
    return () => {
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointerup", handlePointerUp);
      window.removeEventListener("pointercancel", handlePointerUp);
    };
  }, [currentFolderId, onMoveProject, pointerProjectDrag]);

  const currentFolder =
    currentFolderId === null ? null : (foldersById.get(currentFolderId) ?? null);
  const currentFolderIds =
    currentFolder === null ? document.rootFolderIds : currentFolder.childFolderIds;
  const currentProjectPaths =
    currentFolder === null
      ? document.rootProjectPaths
      : currentFolder.projectPaths;
  const currentFolders = currentFolderIds.flatMap((folderId) => {
    const folder = foldersById.get(folderId);
    return folder ? [folder] : [];
  });
  const currentProjects = currentProjectPaths.flatMap((path) => {
    const project = projectsByPath.get(path);
    return project ? [project] : [];
  });
  const moveFolder =
    moveProjectRequest?.folderId === null || !moveProjectRequest
      ? null
      : (foldersById.get(moveProjectRequest.folderId) ?? null);
  const moveFolderIds =
    moveFolder === null ? document.rootFolderIds : moveFolder.childFolderIds;
  const moveFolders = moveFolderIds.flatMap((folderId) => {
    const folder = foldersById.get(folderId);
    return folder ? [folder] : [];
  });
  const moveBreadcrumbs = useMemo(() => {
    return buildProjectBreadcrumbs(
      moveProjectRequest?.folderId ?? null,
      foldersById,
      parentByFolderId,
    );
  }, [foldersById, moveProjectRequest?.folderId, parentByFolderId]);
  const breadcrumbs = useMemo(() => {
    return buildProjectBreadcrumbs(
      currentFolderId,
      foldersById,
      parentByFolderId,
    );
  }, [currentFolderId, foldersById, parentByFolderId]);
  const hasAnyItems =
    document.projects.length > 0 || document.folders.length > 0;

  function submitFolder(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const name = folderName.trim();
    if (name.length === 0) {
      return;
    }
    onCreateFolder(name, currentFolderId);
    setFolderName("");
    setIsCreatingFolder(false);
  }

  function readProjectPath(event: DragEvent) {
    return (
      draggedProjectPathRef.current ||
      draggedProjectPath ||
      event.dataTransfer.getData("application/x-polysmith-project") ||
      event.dataTransfer.getData("text/plain") ||
      ""
    );
  }

  function startProjectDrag(projectPath: string) {
    draggedProjectPathRef.current = projectPath;
    setDraggedProjectPath(projectPath);
  }

  function clearProjectDrag() {
    pointerProjectDragRef.current = null;
    draggedProjectPathRef.current = null;
    setDraggedProjectPath(null);
    setDropTarget(null);
  }

  function canAcceptProjectDrag(event: DragEvent) {
    return (
      draggedProjectPathRef.current !== null ||
      draggedProjectPath !== null ||
      Array.from(event.dataTransfer.types).includes(
        "application/x-polysmith-project",
      ) ||
      Array.from(event.dataTransfer.types).includes("text/plain")
    );
  }

  function dropProjectInto(folderId: string | null, event: DragEvent) {
    event.preventDefault();
    event.stopPropagation();
    const projectPath = readProjectPath(event);
    clearProjectDrag();
    if (projectPath) {
      onMoveProject(projectPath, folderId);
    }
  }

  function dropProjectOnBreadcrumb(folderId: string | null, event: DragEvent) {
    if (!canAcceptProjectDrag(event)) {
      return;
    }
    dropProjectInto(folderId, event);
  }

  function startProjectPointerDrag(
    projectPath: string,
    event: ReactPointerEvent<HTMLDivElement>,
  ) {
    if (event.button !== 0) {
      return;
    }
    const target = event.target;
    if (target instanceof HTMLElement && target.closest("button")) {
      return;
    }
    const nextDrag = {
      projectPath,
      startX: event.clientX,
      startY: event.clientY,
      isDragging: false,
    };
    pointerProjectDragRef.current = nextDrag;
    setPointerProjectDrag(nextDrag);
  }

  function openProjectFromList(project: RecentProject) {
    if (suppressProjectOpenRef.current) {
      suppressProjectOpenRef.current = false;
      return;
    }
    onOpenProject(project);
  }

  function openMoveProject(project: RecentProject) {
    setMoveProjectRequest({
      project,
      folderId: currentFolderId,
    });
  }

  function startProjectContextMenu(project: RecentProject, event: MouseEvent) {
    event.preventDefault();
    setContextMenu({
      x: event.clientX,
      y: event.clientY,
      target: { kind: "project", project },
    });
  }

  function startFolderContextMenu(folder: ProjectFolder, event: MouseEvent) {
    event.preventDefault();
    setContextMenu({
      x: event.clientX,
      y: event.clientY,
      target: { kind: "folder", folder },
    });
  }

  function submitRename(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!renameTarget) {
      return;
    }
    const name = renameTarget.name.trim();
    if (name.length === 0) {
      return;
    }
    if (renameTarget.kind === "project") {
      onRenameProject(renameTarget.project, name);
    } else {
      onRenameFolder(renameTarget.folder.id, name);
    }
    setRenameTarget(null);
  }

  return (
    <section className="cad-scrollbar flex h-full min-h-0 flex-col overflow-y-auto px-2 py-2">
      <div className="cad-projects-toolbar">
        <div className="cad-project-toolbar-row">
          <div className="cad-project-breadcrumbs" aria-label={t("projects.path")}>
            <button
              type="button"
              className={
                dropTarget?.kind === "breadcrumb" && dropTarget.folderId === null
                  ? "cad-project-breadcrumb-button cad-project-breadcrumb-button-drop-target"
                  : "cad-project-breadcrumb-button"
              }
              data-project-breadcrumb-folder-id="root"
              onDragOver={(event) => {
                if (canAcceptProjectDrag(event)) {
                  event.preventDefault();
                  event.dataTransfer.dropEffect = "move";
                  setDropTarget({ kind: "breadcrumb", folderId: null });
                }
              }}
              onDragEnter={(event) => {
                if (canAcceptProjectDrag(event)) {
                  setDropTarget({ kind: "breadcrumb", folderId: null });
                }
              }}
              onDrop={(event) => dropProjectOnBreadcrumb(null, event)}
              onClick={() => setCurrentFolderId(null)}
              aria-label={t("projects.home")}
              title={t("projects.home")}
            >
              <HomeIcon />
            </button>
            {breadcrumbs.map((folder) => (
              <button
                key={folder.id}
                type="button"
                className={
                  dropTarget?.kind === "breadcrumb" &&
                  dropTarget.folderId === folder.id
                    ? "cad-project-breadcrumb-button cad-project-breadcrumb-button-drop-target"
                    : "cad-project-breadcrumb-button"
                }
                data-project-breadcrumb-folder-id={folder.id}
                onDragOver={(event) => {
                  if (canAcceptProjectDrag(event)) {
                    event.preventDefault();
                    event.dataTransfer.dropEffect = "move";
                    setDropTarget({ kind: "breadcrumb", folderId: folder.id });
                  }
                }}
                onDragEnter={(event) => {
                  if (canAcceptProjectDrag(event)) {
                    setDropTarget({ kind: "breadcrumb", folderId: folder.id });
                  }
                }}
                onDrop={(event) => dropProjectOnBreadcrumb(folder.id, event)}
                onClick={() => setCurrentFolderId(folder.id)}
                aria-label={folder.name}
                title={folder.name}
              >
                {folder.name}
              </button>
            ))}
          </div>
          {!isCreatingFolder ? (
            <div className="cad-project-new-menu-wrapper">
              <button
                type="button"
                className="cad-project-new-button"
                onClick={(event) => {
                  event.stopPropagation();
                  setIsNewMenuOpen((current) => !current);
                }}
                aria-label={t("projects.new")}
                title={t("projects.new")}
              >
                <PlusIcon />
              </button>
              {isNewMenuOpen ? (
                <div
                  className="cad-context-menu cad-project-new-menu rounded-xl p-1 shadow-[0_8px_24px_rgba(0,0,0,0.5)] backdrop-blur-xl"
                  onClick={(event) => event.stopPropagation()}
                >
                  <button
                    type="button"
                    className="cad-context-menu-item flex w-full items-center gap-2 rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors"
                    onClick={() => {
                      setIsNewMenuOpen(false);
                      onCreateProject(currentFolderId);
                    }}
                  >
                    <ProjectIcon />
                    {t("projects.newProject")}
                  </button>
                  <button
                    type="button"
                    className="cad-context-menu-item flex w-full items-center gap-2 rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors"
                    onClick={() => {
                      setIsNewMenuOpen(false);
                      setIsCreatingFolder(true);
                    }}
                  >
                    <FolderIcon />
                    {t("projects.newFolder")}
                  </button>
                </div>
              ) : null}
            </div>
          ) : null}
        </div>
        {isCreatingFolder ? (
          <form className="cad-project-folder-form" onSubmit={submitFolder}>
            <input
              className="cad-project-folder-input"
              value={folderName}
              onChange={(event) => setFolderName(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === "Escape") {
                  setIsCreatingFolder(false);
                  setFolderName("");
                }
              }}
              placeholder={t("projects.folderName")}
              autoFocus
            />
            <button type="submit" className="cad-project-folder-submit">
              {t("projects.create")}
            </button>
          </form>
        ) : null}
      </div>

      {!hasAnyItems ? (
        <p className="px-1 py-2 text-sm text-on-surface-muted">
          {t("projects.empty")}
        </p>
      ) : (
        <div
          className={
            draggedProjectPath === null ||
            dropTarget?.kind !== "level" ||
            dropTarget.folderId !== currentFolderId
              ? "cad-project-level"
              : "cad-project-level cad-project-level-drop-target"
          }
          onDragOver={(event) => {
            if (canAcceptProjectDrag(event)) {
              event.preventDefault();
              event.dataTransfer.dropEffect = "move";
              setDropTarget({ kind: "level", folderId: currentFolderId });
            }
          }}
          onDragEnter={(event) => {
            if (canAcceptProjectDrag(event)) {
              setDropTarget({ kind: "level", folderId: currentFolderId });
            }
          }}
          onDrop={(event) => dropProjectInto(currentFolderId, event)}
        >
          {currentFolders.length === 0 && currentProjects.length === 0 ? (
            <p className="px-1 py-1 text-xs text-on-surface-muted">
              {t("projects.dropHere")}
            </p>
          ) : (
            <>
              {currentFolders.map((folder) => (
                <FolderCard
                  key={folder.id}
                  folder={folder}
                  canAcceptDrop={true}
                  isDropTarget={
                    dropTarget?.kind === "folder" &&
                    dropTarget.folderId === folder.id
                  }
                  onOpen={setCurrentFolderId}
                  onDelete={(folderId) => {
                    setDeleteFolderRequest(foldersById.get(folderId) ?? null);
                  }}
                  onContextMenu={startFolderContextMenu}
                  onDragTarget={(folderId) => {
                    setDropTarget({ kind: "folder", folderId });
                  }}
                  onDropProject={(folderId, event) =>
                    dropProjectInto(folderId, event)
                  }
                />
              ))}
              {currentProjects.map((project) => (
                <ProjectCard
                  key={project.path}
                  project={project}
                  isActive={project.path === activeProjectPath}
                  onOpen={openProjectFromList}
                  onContextMenu={startProjectContextMenu}
                  onDelete={(nextProject) =>
                    setDeleteProjectRequest({
                      project: nextProject,
                      shouldDeleteFile: false,
                    })
                  }
                  onPointerDragStart={startProjectPointerDrag}
                  onDragStart={startProjectDrag}
                  onDragEnd={clearProjectDrag}
                />
              ))}
            </>
          )}
        </div>
      )}

      {contextMenu
        ? createPortal(
            <ContextMenuShell
              x={contextMenu.x}
              y={contextMenu.y}
              className="min-w-[140px]"
            >
              <button
                type="button"
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                onClick={() => {
                  if (contextMenu.target.kind === "project") {
                    setRenameTarget({
                      kind: "project",
                      project: contextMenu.target.project,
                      name: contextMenu.target.project.name,
                    });
                  } else {
                    setRenameTarget({
                      kind: "folder",
                      folder: contextMenu.target.folder,
                      name: contextMenu.target.folder.name,
                    });
                  }
                  setContextMenu(null);
                }}
              >
                {t("common.rename")}
              </button>
              {contextMenu.target.kind === "project" ? (
                <button
                  type="button"
                  className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-on-surface transition-colors hover:bg-white/10"
                  onClick={() => {
                    const target = contextMenu.target;
                    if (target.kind === "project") {
                      openMoveProject(target.project);
                    }
                    setContextMenu(null);
                  }}
                >
                  {t("projects.move")}
                </button>
              ) : null}
              <button
                type="button"
                className="flex w-full items-center rounded-lg px-3 py-1.5 text-left text-sm text-danger transition-colors hover:bg-danger/15"
                onClick={() => {
                  if (contextMenu.target.kind === "project") {
                    setDeleteProjectRequest({
                      project: contextMenu.target.project,
                      shouldDeleteFile: false,
                    });
                  } else {
                    setDeleteFolderRequest(contextMenu.target.folder);
                  }
                  setContextMenu(null);
                }}
              >
                {t("common.delete")}
              </button>
            </ContextMenuShell>,
            window.document.body,
          )
        : null}

      {renameTarget ? (
        <div className="cad-modal-backdrop" role="presentation">
          <form
            className="cad-unsaved-dialog"
            role="dialog"
            aria-modal="true"
            aria-labelledby="cad-rename-project-dialog-title"
            onSubmit={submitRename}
          >
            <h2
              id="cad-rename-project-dialog-title"
              className="text-base font-semibold text-on-surface"
            >
              {renameTarget.kind === "project"
                ? t("projects.renameProjectTitle")
                : t("projects.renameFolderTitle")}
            </h2>
            <input
              className="cad-project-rename-input"
              value={renameTarget.name}
              onChange={(event) => {
                setRenameTarget({
                  ...renameTarget,
                  name: event.target.value,
                });
              }}
              autoFocus
            />
            <div className="mt-4 flex justify-end gap-2">
              <button
                type="button"
                className="rounded-md border border-outline-soft px-3 py-1.5 text-sm text-on-surface hover:bg-surface-container"
                onClick={() => setRenameTarget(null)}
              >
                {t("common.cancel")}
              </button>
              <button
                type="submit"
                className="rounded-md bg-primary px-3 py-1.5 text-sm font-semibold text-on-primary"
              >
                {t("common.rename")}
              </button>
            </div>
          </form>
        </div>
      ) : null}

      {moveProjectRequest ? (
        <div className="cad-modal-backdrop" role="presentation">
          <div
            className="cad-unsaved-dialog"
            role="dialog"
            aria-modal="true"
            aria-labelledby="cad-move-project-dialog-title"
          >
            <h2
              id="cad-move-project-dialog-title"
              className="text-base font-semibold text-on-surface"
            >
              {t("projects.moveProjectTitle", {
                name: moveProjectRequest.project.name,
              })}
            </h2>
            <div
              className="cad-project-breadcrumbs mt-3"
              aria-label={t("projects.path")}
            >
              <button
                type="button"
                className="cad-project-breadcrumb-button"
                onClick={() =>
                  setMoveProjectRequest({
                    ...moveProjectRequest,
                    folderId: null,
                  })
                }
                aria-label={t("projects.home")}
                title={t("projects.home")}
              >
                <HomeIcon />
              </button>
              {moveBreadcrumbs.map((folder) => (
                <button
                  key={folder.id}
                  type="button"
                  className="cad-project-breadcrumb-button"
                  onClick={() =>
                    setMoveProjectRequest({
                      ...moveProjectRequest,
                      folderId: folder.id,
                    })
                  }
                  aria-label={folder.name}
                  title={folder.name}
                >
                  {folder.name}
                </button>
              ))}
            </div>
            <div className="cad-project-folder-picker">
              {moveFolders.length === 0 ? (
                <p className="px-1 py-2 text-sm text-on-surface-muted">
                  {t("projects.noFolders")}
                </p>
              ) : (
                moveFolders.map((folder) => (
                  <button
                    key={folder.id}
                    type="button"
                    className="cad-project-folder-picker-item"
                    onClick={() =>
                      setMoveProjectRequest({
                        ...moveProjectRequest,
                        folderId: folder.id,
                      })
                    }
                  >
                    <FolderIcon size={18} />
                    <span className="min-w-0 flex-1 truncate text-left">
                      {folder.name}
                    </span>
                  </button>
                ))
              )}
            </div>
            <div className="mt-4 flex justify-end gap-2">
              <button
                type="button"
                className="rounded-md border border-outline-soft px-3 py-1.5 text-sm text-on-surface hover:bg-surface-container"
                onClick={() => setMoveProjectRequest(null)}
              >
                {t("common.cancel")}
              </button>
              <button
                type="button"
                className="rounded-md bg-primary px-3 py-1.5 text-sm font-semibold text-on-primary"
                onClick={() => {
                  onMoveProject(
                    moveProjectRequest.project.path,
                    moveProjectRequest.folderId,
                  );
                  setMoveProjectRequest(null);
                }}
              >
                {t("projects.moveHere")}
              </button>
            </div>
          </div>
        </div>
      ) : null}

      {deleteProjectRequest ? (
        <div className="cad-modal-backdrop" role="presentation">
          <div
            className="cad-unsaved-dialog"
            role="dialog"
            aria-modal="true"
            aria-labelledby="cad-delete-project-dialog-title"
          >
            <h2
              id="cad-delete-project-dialog-title"
              className="text-base font-semibold text-on-surface"
            >
              {t("projects.deleteProjectTitle", {
                name: deleteProjectRequest.project.name,
              })}
            </h2>
            <p className="mt-2 text-sm text-on-surface-dim">
              {t("projects.deleteProjectBody")}
            </p>
            <label className="cad-project-delete-file-option">
              <Checkbox
                checked={deleteProjectRequest.shouldDeleteFile}
                ariaLabel={t("projects.deleteFileToo")}
                onCheckedChange={(shouldDeleteFile) =>
                  setDeleteProjectRequest({
                    ...deleteProjectRequest,
                    shouldDeleteFile,
                  })
                }
              />
              <span>{t("projects.deleteFileToo")}</span>
            </label>
            <div className="mt-4 flex justify-end gap-2">
              <button
                type="button"
                className="rounded-md border border-outline-soft px-3 py-1.5 text-sm text-on-surface hover:bg-surface-container"
                onClick={() => setDeleteProjectRequest(null)}
              >
                {t("common.cancel")}
              </button>
              <button
                type="button"
                className="rounded-md bg-danger px-3 py-1.5 text-sm font-semibold text-on-primary"
                onClick={() => {
                  onDeleteProject(
                    deleteProjectRequest.project,
                    deleteProjectRequest.shouldDeleteFile,
                  );
                  setDeleteProjectRequest(null);
                }}
              >
                {deleteProjectRequest.shouldDeleteFile
                  ? t("projects.delete")
                  : t("projects.remove")}
              </button>
            </div>
          </div>
        </div>
      ) : null}

      {deleteFolderRequest ? (
        <div className="cad-modal-backdrop" role="presentation">
          <div
            className="cad-unsaved-dialog"
            role="dialog"
            aria-modal="true"
            aria-labelledby="cad-delete-folder-dialog-title"
          >
            <h2
              id="cad-delete-folder-dialog-title"
              className="text-base font-semibold text-on-surface"
            >
              {t("projects.deleteFolderTitle", {
                name: deleteFolderRequest.name,
              })}
            </h2>
            <p className="mt-2 text-sm text-on-surface-dim">
              {t("projects.deleteFolderBody")}
            </p>
            <div className="mt-4 flex justify-end gap-2">
              <button
                type="button"
                className="rounded-md border border-outline-soft px-3 py-1.5 text-sm text-on-surface hover:bg-surface-container"
                onClick={() => setDeleteFolderRequest(null)}
              >
                {t("common.cancel")}
              </button>
              <button
                type="button"
                className="rounded-md bg-danger px-3 py-1.5 text-sm font-semibold text-on-primary"
                onClick={() => {
                  const parentId =
                    parentByFolderId.get(deleteFolderRequest.id) ?? null;
                  onDeleteFolder(deleteFolderRequest.id);
                  if (currentFolderId === deleteFolderRequest.id) {
                    setCurrentFolderId(parentId);
                  }
                  setDeleteFolderRequest(null);
                }}
              >
                {t("projects.delete")}
              </button>
            </div>
          </div>
        </div>
      ) : null}
    </section>
  );
}
