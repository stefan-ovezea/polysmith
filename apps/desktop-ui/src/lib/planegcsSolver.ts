/**
 * planegcsSolver.ts — Lazy singleton for the planegcs WASM solver.
 *
 * Usage:
 *   import { getBridge, ensureBridge } from "@/lib/planegcsSolver";
 *
 *   // At app / component mount:
 *   await ensureBridge();           // loads WASM once
 *
 *   // During drag (synchronous after init):
 *   const bridge = getBridge();
 *   if (bridge) {
 *     const result = bridge.solve(params, constraints);
 *   }
 */

import { PlanegcsBridge, LOOSE, type SolverConfig } from "./planegcsBridge";

// Vite ?url import resolves to the correct URL in both dev and prod:
//   dev  → something like /planegcs.wasm or /@fs/.../planegcs.wasm
//   prod → /assets/planegcs-{hash}.wasm
import planegcsWasmUrl from "@salusoft89/planegcs/dist/planegcs_dist/planegcs.wasm?url";

let _bridge: PlanegcsBridge | null = null;
let _initPromise: Promise<PlanegcsBridge> | null = null;
let _status: "unloaded" | "loading" | "ready" | "error" = "unloaded";
let _error: string | null = null;

/** Return the bridge if already initialised, null otherwise. */
export function getBridge(): PlanegcsBridge | null {
  return _bridge;
}

/** Current WASM solver status for UI indicator / diagnostics. */
export function getSolverStatus(): {
  status: "unloaded" | "loading" | "ready" | "error";
  error: string | null;
} {
  return { status: _status, error: _error };
}

/**
 * Ensure the bridge is initialised (idempotent).
 * Returns the bridge instance once the WASM module is loaded.
 */
export async function ensureBridge(
  config: SolverConfig = LOOSE,
): Promise<PlanegcsBridge> {
  if (_bridge) return _bridge;
  if (!_initPromise) {
    _status = "loading";
    _initPromise = (async () => {
      const b = new PlanegcsBridge(config);
      await b.init(planegcsWasmUrl);
      _bridge = b;
      _status = "ready";
      return b;
    })().catch((err) => {
      _status = "error";
      _error = String(err);
      throw err;
    });
  }
  return _initPromise;
}

/** Set solver config (e.g. switch between LOOSE / EXACT). */
export function setConfig(config: SolverConfig): void {
  if (_bridge) {
    _bridge.config = config;
  }
}
