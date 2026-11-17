// Small pure helpers for the long-operation overlay, kept out of the React
// component so they can be tested without mounting it.

// The trailing path segment. Commands carry absolute OS paths — Windows
// backslashes or POSIX slashes — so both separators split. A path with no
// separator (a bare file name) and a trailing separator both have to come
// back with something displayable rather than an empty string.
export function fileNameFromPath(filePath: string): string {
  const trimmed = filePath.trim();
  if (!trimmed) {
    return "";
  }
  const parts = trimmed.split(/[\\/]/).filter((part) => part.length > 0);
  return parts.length > 0 ? parts[parts.length - 1] : trimmed;
}

// Whole seconds since the operation started. Clamped at zero because the
// overlay ticks off a wall clock, and a system clock adjustment backwards
// must not render "-3 s".
export function elapsedSeconds(startedAt: number, now: number): number {
  if (!Number.isFinite(startedAt) || !Number.isFinite(now)) {
    return 0;
  }
  return Math.max(0, Math.floor((now - startedAt) / 1000));
}
