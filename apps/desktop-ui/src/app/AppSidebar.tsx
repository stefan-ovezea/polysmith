import type { ComponentProps, Dispatch, SetStateAction } from "react";
import { useTranslation } from "react-i18next";

import {
  CamOperationPanel,
  DocumentHierarchyPanel,
  ProjectsPanel,
} from "../layout";
import type { CategoryId } from "../layout";
import type { CamOperation } from "../layout/CamOperationPanel";
import { SidebarResizer } from "../layout/SidebarResizer";
import type { RecentProject, RecentProjectsDocument } from "../lib";
import type { DocumentState } from "../types";
import type { PendingUnsavedAction, SidebarTab, WorkspaceView } from "./appState";

type RunAction = (action: () => Promise<void>) => Promise<void>;

type BodyContextActions = Pick<
  ComponentProps<typeof DocumentHierarchyPanel>,
  "onMoveBody" | "onCopyBody" | "onExportBodyMesh" | "onUnlinkBodyCopy"
>;

interface AppSidebarProps {
  activeProjectPath: string | null;
  bodyContextActions: BodyContextActions;
  camOperationDelete: (operationId: string) => Promise<void>;
  camOperations: CamOperation[];
  confirmAndDeleteFeature: (featureId: string) => void;
  createRecentProjectFolder: (
    name: string,
    parentFolderId: string | null,
  ) => Promise<void>;
  deleteRecentProject: (
    project: RecentProject,
    shouldDeleteFile: boolean,
  ) => Promise<void>;
  deleteRecentProjectFolder: (folderId: string) => Promise<void>;
  document: DocumentState | null;
  hiddenCategories: Set<CategoryId>;
  hiddenFeatureIds: Set<string>;
  hierarchyWidth: number;
  isHierarchyCollapsed: boolean;
  markOriginVisibilityChanged: () => void;
  moveRecentProject: (
    projectPath: string,
    folderId: string | null,
  ) => Promise<void>;
  recentProjectsDocument: RecentProjectsDocument;
  reenterSketch: (featureId: string) => Promise<void>;
  renameFeature: (featureId: string, name: string) => Promise<void>;
  renameRecentProjectEntry: (
    project: RecentProject,
    name: string,
  ) => Promise<void>;
  renameRecentProjectFolder: (
    folderId: string,
    name: string,
  ) => Promise<void>;
  requestOpenRecentProject: (project: RecentProject) => Promise<void>;
  requestUnsavedGate: (action: PendingUnsavedAction) => void;
  runAction: RunAction;
  selectedCamOperationId: string | null;
  selectFeature: (featureId: string) => Promise<void>;
  selectReference: (referenceId: string) => Promise<void>;
  setHiddenCategories: Dispatch<SetStateAction<Set<CategoryId>>>;
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setHierarchyWidth: (width: number) => void;
  setIsHierarchyCollapsed: (collapsed: boolean) => void;
  setSelectedCamOperationId: (operationId: string | null) => void;
  setFeatureSuppressed: (
    featureId: string,
    suppressed: boolean,
  ) => Promise<void>;
  setSidebarTab: (tab: SidebarTab) => void;
  sidebarTab: SidebarTab;
  workspaceView: WorkspaceView;
}

export function AppSidebar({
  activeProjectPath,
  bodyContextActions,
  camOperationDelete,
  camOperations,
  confirmAndDeleteFeature,
  createRecentProjectFolder,
  deleteRecentProject,
  deleteRecentProjectFolder,
  document,
  hiddenCategories,
  hiddenFeatureIds,
  hierarchyWidth,
  isHierarchyCollapsed,
  markOriginVisibilityChanged,
  moveRecentProject,
  recentProjectsDocument,
  reenterSketch,
  renameFeature,
  renameRecentProjectEntry,
  renameRecentProjectFolder,
  requestOpenRecentProject,
  requestUnsavedGate,
  runAction,
  selectedCamOperationId,
  selectFeature,
  selectReference,
  setFeatureSuppressed,
  setHiddenCategories,
  setHiddenFeatureIds,
  setHierarchyWidth,
  setIsHierarchyCollapsed,
  setSelectedCamOperationId,
  setSidebarTab,
  sidebarTab,
  workspaceView,
}: AppSidebarProps) {
  const { t } = useTranslation();

  if (workspaceView === "cam") {
    return (
      <aside
        className="cad-sidebar relative min-h-0 flex-shrink-0"
        style={{ width: hierarchyWidth }}
      >
        <CamOperationPanel
          operations={camOperations}
          selectedOperationId={selectedCamOperationId}
          onSelectOperation={setSelectedCamOperationId}
          onDeleteOperation={(operationId) => {
            void runAction(async () => {
              await camOperationDelete(operationId);
              if (selectedCamOperationId === operationId) {
                setSelectedCamOperationId(null);
              }
            });
          }}
        />
        <SidebarResizer width={hierarchyWidth} onResize={setHierarchyWidth} />
      </aside>
    );
  }

  if (isHierarchyCollapsed) {
    return (
      <button
        type="button"
        className="cad-sidebar-collapsed"
        onClick={() => setIsHierarchyCollapsed(false)}
        aria-label={t("document.expandHierarchyPanel")}
        title={t("document.expandHierarchy")}
      >
        <span className="cad-sidebar-collapsed-label">
          {t("document.hierarchyProjects")}
        </span>
      </button>
    );
  }

  return (
    <aside
      className="cad-sidebar relative min-h-0 flex-shrink-0"
      style={{ width: hierarchyWidth }}
    >
      <div className="flex h-full min-h-0 flex-col">
        <div className="flex items-center justify-between gap-2 px-3 pt-2">
          <div className="cad-sidebar-tabs" role="tablist">
            <button
              type="button"
              className={
                sidebarTab === "projects"
                  ? "cad-sidebar-tab cad-sidebar-tab-active"
                  : "cad-sidebar-tab"
              }
              onClick={() => setSidebarTab("projects")}
              role="tab"
              aria-selected={sidebarTab === "projects"}
            >
              {t("projects.title")}
            </button>
            <button
              type="button"
              className={
                sidebarTab === "hierarchy"
                  ? "cad-sidebar-tab cad-sidebar-tab-active"
                  : "cad-sidebar-tab"
              }
              onClick={() => setSidebarTab("hierarchy")}
              role="tab"
              aria-selected={sidebarTab === "hierarchy"}
            >
              {t("document.hierarchy")}
            </button>
          </div>
          <button
            type="button"
            className="cad-sidebar-collapse-button"
            onClick={() => setIsHierarchyCollapsed(true)}
            aria-label={t("document.collapseHierarchyPanel")}
            title={t("document.collapse")}
          >
            ◀
          </button>
        </div>
        {sidebarTab === "hierarchy" ? (
          <DocumentHierarchyPanel
            document={document}
            hiddenFeatureIds={hiddenFeatureIds}
            hiddenCategories={hiddenCategories}
            onToggleFeatureVisibility={(featureId) => {
              setHiddenFeatureIds((current) => {
                const next = new Set(current);
                if (next.has(featureId)) {
                  next.delete(featureId);
                } else {
                  next.add(featureId);
                }
                return next;
              });
            }}
            onToggleCategoryVisibility={(category) => {
              if (category === "origin") {
                markOriginVisibilityChanged();
              }
              setHiddenCategories((current) => {
                const next = new Set(current);
                if (next.has(category)) {
                  next.delete(category);
                } else {
                  next.add(category);
                }
                return next;
              });
            }}
            onSelectFeature={async (featureId) => {
              await runAction(async () => {
                await selectFeature(featureId);
              });
            }}
            onSelectReference={async (referenceId) => {
              await runAction(async () => {
                await selectReference(referenceId);
              });
            }}
            onReenterSketch={async (featureId) => {
              await runAction(async () => {
                await reenterSketch(featureId);
              });
            }}
            onRenameFeature={async (featureId, name) => {
              await runAction(async () => {
                await renameFeature(featureId, name);
              });
            }}
            onDeleteFeature={async (featureId) => {
              confirmAndDeleteFeature(featureId);
            }}
            {...bodyContextActions}
            onSetFeatureSuppressed={async (featureId, suppressed) => {
              await runAction(async () => {
                await setFeatureSuppressed(featureId, suppressed);
              });
            }}
          />
        ) : (
          <ProjectsPanel
            document={recentProjectsDocument}
            activeProjectPath={activeProjectPath}
            onOpenProject={(project) => {
              void runAction(async () => {
                await requestOpenRecentProject(project);
              });
            }}
            onCreateFolder={(name, parentFolderId) => {
              void runAction(async () => {
                await createRecentProjectFolder(name, parentFolderId);
              });
            }}
            onMoveProject={(projectPath, folderId) => {
              void runAction(async () => {
                await moveRecentProject(projectPath, folderId);
              });
            }}
            onDeleteProject={(project, shouldDeleteFile) => {
              void runAction(async () => {
                await deleteRecentProject(project, shouldDeleteFile);
              });
            }}
            onDeleteFolder={(folderId) => {
              void runAction(async () => {
                await deleteRecentProjectFolder(folderId);
              });
            }}
            onRenameProject={(project, name) => {
              void runAction(async () => {
                await renameRecentProjectEntry(project, name);
              });
            }}
            onRenameFolder={(folderId, name) => {
              void runAction(async () => {
                await renameRecentProjectFolder(folderId, name);
              });
            }}
            onCreateProject={(parentFolderId) => {
              requestUnsavedGate({
                kind: "newProject",
                parentFolderId,
              });
            }}
          />
        )}
      </div>
      <SidebarResizer width={hierarchyWidth} onResize={setHierarchyWidth} />
    </aside>
  );
}
