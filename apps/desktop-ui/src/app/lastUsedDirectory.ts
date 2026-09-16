const LAST_USED_DIRECTORY_KEY = "polysmith-last-used-directory";

export function directoryFromFilePath(filePath: string): string | null {
  const separatorIndex = Math.max(
    filePath.lastIndexOf("/"),
    filePath.lastIndexOf("\\"),
  );
  return separatorIndex > 0 ? filePath.slice(0, separatorIndex) : null;
}

export function readLastUsedDirectory(): string | null {
  try {
    const stored = localStorage.getItem(LAST_USED_DIRECTORY_KEY);
    if (stored && stored.length > 0) {
      return stored;
    }
  } catch {
    // Corrupt storage should not block the dialogs.
  }
  return null;
}

export function writeLastUsedDirectory(directory: string | null): void {
  try {
    if (directory) {
      localStorage.setItem(LAST_USED_DIRECTORY_KEY, directory);
    } else {
      localStorage.removeItem(LAST_USED_DIRECTORY_KEY);
    }
  } catch {
    // Persistence is best-effort; dialogs still work without it.
  }
}
