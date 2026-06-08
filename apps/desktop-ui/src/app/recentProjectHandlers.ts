import {
  deleteProjectFile,
  projectFileExists,
  type RecentProject,
} from "../lib";
import type {
  PendingUnsavedAction,
  SavedDocumentBaseline,
} from "./appState";
import * as recentProjectActions from "./recentProjectActions";
import type { RecentProjectsStore } from "./recentProjectActions";

interface RecentProjectHandlersContext {
  recentProjectsStore: RecentProjectsStore;
  currentProjectPath: string | null;
  setCurrentProjectPath: (nextPath: string | null) => void;
  setSavedDocumentBaseline: (nextBaseline: SavedDocumentBaseline | null) => void;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
  requestUnsavedGate: (action: PendingUnsavedAction) => void;
}

export function createRecentProjectHandlers({
  recentProjectsStore,
  currentProjectPath,
  setCurrentProjectPath,
  setSavedDocumentBaseline,
  addMessage,
  translate,
  requestUnsavedGate,
}: RecentProjectHandlersContext) {
  async function recordRecentProject(
    filePath: string,
    thumbnailDataUrl: string | null,
    parentFolderId?: string | null,
  ) {
    await recentProjectActions.recordRecentProject(
      recentProjectsStore,
      filePath,
      thumbnailDataUrl,
      parentFolderId,
    );
  }

  async function createRecentProjectFolder(
    name: string,
    parentFolderId: string | null,
  ) {
    await recentProjectActions.createRecentProjectFolder(
      recentProjectsStore,
      name,
      parentFolderId,
    );
  }

  async function moveRecentProject(
    projectPath: string,
    folderId: string | null,
  ) {
    await recentProjectActions.moveRecentProject(
      recentProjectsStore,
      projectPath,
      folderId,
    );
  }

  async function renameRecentProjectEntry(
    project: RecentProject,
    name: string,
  ) {
    await recentProjectActions.renameRecentProjectEntry(
      recentProjectsStore,
      project,
      name,
    );
  }

  async function deleteRecentProject(
    project: RecentProject,
    shouldDeleteFile: boolean,
  ) {
    if (shouldDeleteFile) {
      await deleteProjectFile(project.path);
      if (project.path === currentProjectPath) {
        setCurrentProjectPath(null);
        setSavedDocumentBaseline(null);
      }
    }
    await recentProjectActions.removeRecentProjectEntry(
      recentProjectsStore,
      project,
    );
  }

  async function deleteRecentProjectFolder(folderId: string) {
    await recentProjectActions.deleteRecentProjectFolder(
      recentProjectsStore,
      folderId,
    );
  }

  async function renameRecentProjectFolder(folderId: string, name: string) {
    await recentProjectActions.renameRecentProjectFolder(
      recentProjectsStore,
      folderId,
      name,
    );
  }

  async function requestOpenRecentProject(project: RecentProject) {
    if (project.path === currentProjectPath) {
      return;
    }
    let exists = false;
    try {
      exists = await projectFileExists(project.path);
    } catch (error) {
      addMessage(`project file check error: ${String(error)}`);
      return;
    }
    if (!exists) {
      addMessage(translate("projects.openMissingFile", { name: project.name }));
      await deleteRecentProject(project, false);
      return;
    }
    requestUnsavedGate({
      kind: "load",
      filePath: project.path,
    });
  }

  return {
    createRecentProjectFolder,
    deleteRecentProject,
    deleteRecentProjectFolder,
    moveRecentProject,
    recordRecentProject,
    renameRecentProjectEntry,
    renameRecentProjectFolder,
    requestOpenRecentProject,
  };
}
