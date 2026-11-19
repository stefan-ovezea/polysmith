// Coalesce concurrent invocations of an async operation into one run.
//
// Used for starting the CAD core: React 18 StrictMode mounts effects twice in
// development, and the header's Start button can be clicked while the automatic
// start is still in flight. Both produced a second `start_cad_core` call, and the
// Rust side — which restarts the core when one is already running — killed the
// first core and launched another ("Starting cad_core from ..." twice, with
// "cad_core already running — restarting with the current binary" between them).
// Any command already sent to the first core died with it.
//
// Module scope matters: the guard has to survive a StrictMode remount, so this
// is a function of the operation, not of a component ref.
export function coalesceAsync<T>(operation: () => Promise<T>): () => Promise<T> {
  let inFlight: Promise<T> | null = null;
  return () => {
    if (inFlight !== null) {
      return inFlight;
    }
    // `finally` (not `then`) so a rejection clears the slot too — a failed start
    // must be retryable.
    inFlight = operation().finally(() => {
      inFlight = null;
    });
    return inFlight;
  };
}
