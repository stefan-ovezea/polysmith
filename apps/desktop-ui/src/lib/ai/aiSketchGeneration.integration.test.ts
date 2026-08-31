// @vitest-environment node
// Opt-in integration suite: drives the REAL cad_core binary through
// Ollama-generated sketch scenarios and asserts the serialized document state
// (profiles, extrudes, bodies). Local models are nondeterministic, so every
// scenario retries up to 3 times with a fresh core per attempt.
//
// Prerequisites:
//   pnpm core:build          (build the native core; suite self-skips without it)
//   Ollama running with a model pulled (default gemma4:12b), OR
//   AI_PROVIDER=deepseek with AI_API_KEY set (cloud API — scenarios run in
//   seconds instead of minutes on CPU)
// Env overrides: AI_PROVIDER, AI_API_KEY, AI_MODEL, AI_BASE_URL, AI_API_STYLE,
// POLYSMITH_CORE_BIN
import { existsSync } from "node:fs";

import { beforeAll, describe, expect, it } from "vitest";

import { CadCoreHarness, coreBinaryPath } from "./cadCoreHarness";
import {
  assertProfilesMatch,
  expectBodyCount,
  expectExtrudeDepth,
  extrudeFeatures,
  featureKinds,
} from "./profileAssertions";
import {
  loadAiSettingsFromHome,
  runAiScenario,
  type AiScenarioConfig,
  type ScenarioResult,
} from "./runAiScenario";

const CORE_EXE = coreBinaryPath();
const PROVIDER = (process.env.AI_PROVIDER ?? "ollama") as "ollama" | "deepseek";
const MODEL =
  process.env.AI_MODEL ??
  (PROVIDER === "deepseek" ? "deepseek-v4-pro[1m]" : "gemma4:12b");
const BASE_URL =
  process.env.AI_BASE_URL ??
  (PROVIDER === "deepseek"
    ? "https://api.deepseek.com/anthropic"
    : "http://localhost:11434");
const API_KEY = process.env.AI_API_KEY ?? "";
const API_STYLE = (process.env.AI_API_STYLE ?? "anthropic") as
  | "anthropic"
  | "openai";
const MAX_FLAKE_ATTEMPTS = 3;

if (!existsSync(CORE_EXE)) {
  // eslint-disable-next-line no-console
  console.warn(
    `[ai-integration] core binary not found at ${CORE_EXE} — run "pnpm core:build" first.`,
  );
}

async function ollamaReady(): Promise<boolean> {
  try {
    const response = await fetch(`${BASE_URL}/api/tags`);
    if (!response.ok) {
      return false;
    }
    const payload = (await response.json()) as { models?: Array<{ name?: string }> };
    return (payload.models ?? []).some((entry) => entry.name === MODEL);
  } catch {
    return false;
  }
}

// Rethrows an assertion failure with the scenario's model envelopes and
// recovery errors attached — debuggability is the whole point of the harness.
function withEnvelopes(result: ScenarioResult, error: unknown): Error {
  const message = error instanceof Error ? error.message : String(error);
  const lines = [
    message,
    ...result.errors.map((entry) => `  recovery: ${entry}`),
    "  last model envelopes:",
    ...result.envelopes.slice(-2).map((envelope) => `    ${envelope.slice(0, 400)}`),
  ];
  return new Error(lines.join("\n"));
}

function scenarioConfig(): AiScenarioConfig {
  return {
    provider: PROVIDER,
    model: MODEL,
    baseUrl: BASE_URL,
    apiKey: API_KEY,
    apiStyle: API_STYLE,
    // eslint-disable-next-line no-console
    progress: (message) => console.warn(`[ai] ${message}`),
  };
}

describe.skipIf(!existsSync(CORE_EXE))("AI sketch generation", () => {
  beforeAll(async () => {
    if (PROVIDER === "deepseek") {
      // Mirror the harness resolution order: env var, then ~/.polysmith.
      const fromHome = loadAiSettingsFromHome();
      if (!API_KEY && !fromHome.apiKey) {
        throw new Error(
          'AI_PROVIDER=deepseek needs a key: set AI_API_KEY or add "deepseek_api_key" to ~/.polysmith.',
        );
      }
      return;
    }
    if (!(await ollamaReady())) {
      throw new Error(
        `Ollama is not serving model "${MODEL}" at ${BASE_URL}. ` +
          `Start Ollama and pull the model, or set AI_MODEL.`,
      );
    }
  }, 30000);

  async function withFlakeRetries<T>(
    label: string,
    scenarioFn: (core: CadCoreHarness) => Promise<T>,
  ): Promise<T> {
    let lastError: Error | undefined;
    for (let attempt = 1; attempt <= MAX_FLAKE_ATTEMPTS; attempt++) {
      const core = new CadCoreHarness();
      try {
        return await scenarioFn(core);
      } catch (error) {
        lastError = error instanceof Error ? error : new Error(String(error));
        // eslint-disable-next-line no-console
        console.warn(
          `[flake] ${label} attempt ${attempt}/${MAX_FLAKE_ATTEMPTS} failed: ${lastError.message}`,
        );
      } finally {
        await core.close();
      }
    }
    throw lastError ?? new Error(`${label}: no attempt ran`);
  }

  it(
    "creates a rectangle sketch and extrudes it",
    async () => {
      await withFlakeRetries("rectangle extrude", async (core) => {
        const result = await runAiScenario(
          "Create a 60x40 rectangle on XY and extrude 20mm.",
          scenarioConfig(),
          core,
        );
        try {
          expect(result.document).toBeTruthy();
          const kinds = featureKinds(result.document!);
          expect(kinds).toContain("sketch");
          expect(kinds).toContain("extrude");
          assertProfilesMatch(result.document!, [
            { kind: "polygon", boundary_count: 4, has_source_circle_id: false },
          ]);
          expectExtrudeDepth(result.document!, 20);
          expectBodyCount(result.viewport!, 1);
        } catch (error) {
          throw withEnvelopes(result, error);
        }
      });
    },
    600_000,
  );

  it(
    "cuts two circular holes through an extruded plate",
    async () => {
      await withFlakeRetries("two-circle cut", async (core) => {
        const result = await runAiScenario(
          "Create a 60x40 rectangle on XY and extrude it 20mm. Then cut two 8mm diameter circles through the plate.",
          scenarioConfig(),
          core,
        );
        try {
          const document = result.document;
          expect(document).toBeTruthy();
          const cut = extrudeFeatures(document!).find(
            (parameters) =>
              parameters.operation === "cut" || parameters.mode === "cut",
          );
          expect(cut).toBeTruthy();
          expectBodyCount(result.viewport!, 1);
          // The plate region keeps its 4 line boundary; each full circle also
          // appears as its own region. Circle regions serialize differently
          // across arrangements ("polygon" with source_circle_id, or "circle"
          // without) — accept both.
          assertProfilesMatch(document!, [
            { kind: "polygon", boundary_count: 4, has_source_circle_id: false },
            { kind: ["circle", "polygon"], boundary_count: 1 },
            { kind: ["circle", "polygon"], boundary_count: 1 },
          ]);
        } catch (error) {
          throw withEnvelopes(result, error);
        }
      });
    },
    600_000,
  );

  it(
    "creates a slot sketch and extrudes it",
    async () => {
      await withFlakeRetries("slot extrude", async (core) => {
        const result = await runAiScenario(
          "Create a slot 24mm long and 8mm wide centered at the origin on XY and extrude 5mm.",
          scenarioConfig(),
          core,
        );
        try {
          const document = result.document;
          expect(document).toBeTruthy();
          const kinds = featureKinds(document!);
          expect(kinds).toContain("sketch");
          expect(kinds).toContain("extrude");
          // Slot boundaries include generated arcs; keep the assertion at
          // count + kind level (exact boundary sets proved brittle across
          // the generated entity ids).
          assertProfilesMatch(document!, [{ kind: "polygon" }]);
          expectExtrudeDepth(document!, 5);
          expectBodyCount(result.viewport!, 1);
        } catch (error) {
          throw withEnvelopes(result, error);
        }
      });
    },
    600_000,
  );
});
