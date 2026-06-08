import type { ProjectFolder } from "@/lib";

export function buildProjectBreadcrumbs(
  folderId: string | null,
  foldersById: ReadonlyMap<string, ProjectFolder>,
  parentByFolderId: ReadonlyMap<string, string | null>,
) {
  const breadcrumbs: ProjectFolder[] = [];
  const visited = new Set<string>();
  let currentFolderId = folderId;
  while (currentFolderId !== null && !visited.has(currentFolderId)) {
    visited.add(currentFolderId);
    const folder = foldersById.get(currentFolderId);
    if (!folder) {
      break;
    }
    breadcrumbs.unshift(folder);
    currentFolderId = parentByFolderId.get(currentFolderId) ?? null;
  }
  return breadcrumbs;
}
