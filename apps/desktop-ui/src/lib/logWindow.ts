import type { LogEntry } from "@/types";

// The Logs panel is the only diagnostics surface this app has, so the window it
// keeps has to be biased: an hour of info chatter must never push out the
// warning that explains a failure.
export const LOG_WINDOW_SIZE = 500;

function isDiagnostic(entry: LogEntry): boolean {
  return entry.level === "warn" || entry.level === "error";
}

/**
 * Appends one entry and trims back to the window size.
 *
 * While there is anything to give, the OLDEST non-diagnostic entries (debug and
 * info) go first; warnings and errors are only evicted once the window holds
 * nothing else, oldest first.  Order is always preserved.
 */
export function appendLogWithinWindow(
  logs: LogEntry[],
  entry: LogEntry,
  windowSize: number = LOG_WINDOW_SIZE,
): LogEntry[] {
  const next = [...logs, entry];
  if (next.length <= windowSize) {
    return next;
  }
  const excess = next.length - windowSize;
  const dropped = new Set<number>();
  // Pass 1: the oldest entries that are not diagnostics.
  for (let index = 0; index < next.length && dropped.size < excess; index += 1) {
    if (!isDiagnostic(next[index])) {
      dropped.add(index);
    }
  }
  // Pass 2: the cap forces it — nothing but diagnostics left.
  for (let index = 0; index < next.length && dropped.size < excess; index += 1) {
    dropped.add(index);
  }
  return next.filter((_, index) => !dropped.has(index));
}
