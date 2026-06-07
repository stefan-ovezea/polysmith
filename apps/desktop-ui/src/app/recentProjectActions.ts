import {
  createProjectFolder,
  deleteProjectFolder,
  moveProjectToFolder,
  removeProjectFromRecentProjects,
  renameProjectFolder,
  renameRecentProject,
  saveRecentProjects,
  upsertRecentProject,
} from "../lib";
import type { RecentProject, RecentProjectsDocument } from "../lib";

export interface RecentProjectsStore {
  read: () => RecentProjectsDocument;
  write: (nextDocument: RecentProjectsDocument) => void;
}

export async function updateRecentProjectsDocument(
  store: RecentProjectsStore,
  nextDocument: RecentProjectsDocument,
) {
  store.write(nextDocument);
  await saveRecentProjects(nextDocument);
}

export async function recordRecentProject(
  store: RecentProjectsStore,
  filePath: string,
  thumbnailDataUrl: string | null,
  parentFolderId?: string | null,
) {
  const baseDocument = store.read();
  const existing = baseDocument.projects.find(
    (project) => project.path === filePath,
  );
  await updateRecentProjectsDocument(
    store,
    upsertRecentProject(baseDocument, {
      path: filePath,
      name: existing?.name,
      thumbnailDataUrl: thumbnailDataUrl ?? existing?.thumbnailDataUrl ?? null,
      parentFolderId,
    }),
  );
}

export async function createRecentProjectFolder(
  store: RecentProjectsStore,
  name: string,
  parentFolderId: string | null,
) {
  await updateRecentProjectsDocument(
    store,
    createProjectFolder(store.read(), name, parentFolderId),
  );
}

export async function moveRecentProject(
  store: RecentProjectsStore,
  projectPath: string,
  folderId: string | null,
) {
  await updateRecentProjectsDocument(
    store,
    moveProjectToFolder(store.read(), projectPath, folderId),
  );
}

export async function renameRecentProjectEntry(
  store: RecentProjectsStore,
  project: RecentProject,
  name: string,
) {
  await updateRecentProjectsDocument(
    store,
    renameRecentProject(store.read(), project.path, name),
  );
}

export async function removeRecentProjectEntry(
  store: RecentProjectsStore,
  project: RecentProject,
) {
  await updateRecentProjectsDocument(
    store,
    removeProjectFromRecentProjects(store.read(), project.path),
  );
}

export async function deleteRecentProjectFolder(
  store: RecentProjectsStore,
  folderId: string,
) {
  await updateRecentProjectsDocument(
    store,
    deleteProjectFolder(store.read(), folderId),
  );
}

export async function renameRecentProjectFolder(
  store: RecentProjectsStore,
  folderId: string,
  name: string,
) {
  await updateRecentProjectsDocument(
    store,
    renameProjectFolder(store.read(), folderId, name),
  );
}
