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

// The stage line tells the user *what* is happening. This turns the core's own
// progress lines into a *fraction* so the overlay can show a bar instead of a
// spinner for the long phases.
//
// The core logs "Writing part 273/1092" while it builds bodies (one per solid),
// which is the only truly countable phase of an import. Everything before it —
// parsing, the XCAF transfer, flattening — is one opaque OCCT call, so those
// stay indeterminate; the elapsed clock covers them.
export interface OperationProgress {
  current: number;
  total: number;
  // 0..1, clamped. A total of 0 cannot be divided, so it reads as "no
  // progress" rather than NaN.
  fraction: number;
}

// "Writing part 273/1092" — the body-build phase.
const WRITING_PART = /Writing part\s+(\d+)\s*\/\s*(\d+)/i;

export function parseOperationProgress(
  detail: string,
): OperationProgress | null {
  if (!detail) {
    return null;
  }
  const match = WRITING_PART.exec(detail);
  if (!match) {
    return null;
  }
  const current = Number(match[1]);
  const total = Number(match[2]);
  if (!Number.isFinite(current) || !Number.isFinite(total) || total <= 0) {
    return null;
  }
  const clampedCurrent = Math.max(0, Math.min(current, total));
  return {
    current: clampedCurrent,
    total,
    fraction: clampedCurrent / total,
  };
}
