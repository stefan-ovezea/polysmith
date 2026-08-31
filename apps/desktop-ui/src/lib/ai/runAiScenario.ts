// Headless AI agent loop for the sketch-generation tests: asks the model for
// a command envelope, validates and executes it against a CadCoreHarness,
// feeds failures back as a bounded recovery turn, and continues until the
// model signals completion or the step budget runs out. Reuses the exact same
// prompt builders, envelope parser and batch validation as the in-app AI
// assistant — zero duplication.
// Node-only module (imported by the integration tests, never by app code).
import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

import type { AiConfig } from "@/config";
import type { DocumentState, ViewportState } from "@/types";

import {
  buildAiCadRecoveryPrompt,
  buildAiCadSystemPrompt,
  buildAiCadUserPrompt,
} from "../aiCadPrompt";
import {
  formatAiCommandError,
  parseAiCommandEnvelope,
  prepareAiCommandBatchForState,
  type PreparedAiCommandBatch,
} from "../aiCommandProtocol";
import { requestAiChat } from "../aiClient";
import { CadCoreHarness } from "./cadCoreHarness";

export interface AiScenarioConfig {
  /** Provider default: "ollama". DeepSeek scenarios need apiKey. */
  provider?: "ollama" | "deepseek";
  model?: string;
  baseUrl?: string;
  apiKey?: string;
  apiStyle?: "anthropic" | "openai";
  maxAgentSteps?: number;
  maxRecoveryAttempts?: number;
  /** Optional progress sink — the scenarios are multi-minute; without this
   * a timeout leaves no trace of where the loop was. */
  progress?: (message: string) => void;
}

export interface ScenarioResult {
  document: DocumentState | null;
  viewport: ViewportState | null;
  /** Raw model envelope JSON per model call, for failure debugging. */
  envelopes: string[];
  /** Validation/core failures that were fed back to the model. */
  errors: string[];
  steps: number;
}

type ChatMessage = { role: "system" | "user" | "assistant"; content: string };

function resolveHome(): string {
  // USERPROFILE is always Windows-style (C:\Users\...); HOME inside Git Bash
  // is MSYS-style (/c/Users/...) which node's fs does NOT resolve on Windows.
  if (process.platform === "win32" && process.env.USERPROFILE) {
    return process.env.USERPROFILE;
  }
  return homedir();
}

// Same rule as the app: the key never lives in the repo or the config — for
// the harness it comes from AI_API_KEY or the user's ~/.polysmith file, which
// may also carry the deepseek base URL.
export function loadAiSettingsFromHome(): { apiKey: string; baseUrl: string } {
  try {
    const raw = readFileSync(join(resolveHome(), ".polysmith"), "utf8");
    const parsed = JSON.parse(raw) as {
      deepseek_api_key?: string;
      deepseek_base_url?: string;
    };
    return {
      apiKey: parsed.deepseek_api_key?.trim() ?? "",
      baseUrl: parsed.deepseek_base_url?.trim() ?? "",
    };
  } catch {
    return { apiKey: "", baseUrl: "" };
  }
}

function makeAiConfig(config: AiScenarioConfig): AiConfig {
  const provider = config.provider ?? "ollama";
  const defaults =
    provider === "deepseek"
      ? {
          baseUrl: "https://api.deepseek.com/anthropic",
          model: "deepseek-v4-pro[1m]",
        }
      : { baseUrl: "http://localhost:11434", model: "gemma4:12b" };
  const fromHome = provider === "deepseek" ? loadAiSettingsFromHome() : null;
  return {
    enabled: true,
    provider,
    baseUrl:
      config.baseUrl ??
      process.env.AI_BASE_URL ??
      fromHome?.baseUrl ??
      defaults.baseUrl,
    model: config.model ?? defaults.model,
    // NOTE: `||`-style fallback on purpose — an empty-string config value
    // (e.g. unset env passed through by the test) must NOT stop the chain.
    apiKey:
      (config.apiKey && config.apiKey.trim()) ||
      process.env.AI_API_KEY?.trim() ||
      fromHome?.apiKey ||
      "",
    apiStyle: config.apiStyle ?? "anthropic",
    previewBeforeRun: false,
    maxAgentSteps: config.maxAgentSteps ?? 8,
  };
}

function buildMessages(
  prompt: string,
  core: CadCoreHarness,
  failureText: string | undefined,
): ChatMessage[] {
  const messages: ChatMessage[] = [
    { role: "system", content: buildAiCadSystemPrompt() },
    {
      role: "user",
      content: buildAiCadUserPrompt(
        prompt,
        core.latestDocument(),
        core.latestViewport(),
      ),
    },
  ];
  if (failureText) {
    messages.push({
      role: "user",
      content: buildAiCadRecoveryPrompt(failureText),
    });
  }
  return messages;
}

// Executes a prepared batch command by command. Returns a failure
// description (command label + error) or null when the batch ran cleanly.
async function executeBatch(
  core: CadCoreHarness,
  batch: PreparedAiCommandBatch,
): Promise<string | null> {
  for (const command of batch.commands) {
    try {
      await core.send(command.type, command.payload);
    } catch (error) {
      return `${command.type}: ${formatAiCommandError(error)}`;
    }
  }
  return null;
}

async function refreshState(core: CadCoreHarness): Promise<void> {
  await core.send("get_document_state");
  await core.send("get_viewport_state");
}

function makeResult(
  core: CadCoreHarness,
  envelopes: string[],
  errors: string[],
  steps: number,
): ScenarioResult {
  return {
    document: core.latestDocument(),
    viewport: core.latestViewport(),
    envelopes,
    errors,
    steps,
  };
}

export async function runAiScenario(
  prompt: string,
  config: AiScenarioConfig,
  core: CadCoreHarness,
): Promise<ScenarioResult> {
  const aiConfig = makeAiConfig(config);
  const maxRecovery = config.maxRecoveryAttempts ?? 3;
  const envelopes: string[] = [];
  const errors: string[] = [];

  let step = 1;
  while (step <= aiConfig.maxAgentSteps) {
    let failureText: string | undefined;
    let completed = false;

    // One agent step: ask the model until a batch executes cleanly or the
    // recovery budget for this step runs out.
    for (let attempt = 0; attempt <= maxRecovery; attempt++) {
      config.progress?.(
        `step ${step}/${aiConfig.maxAgentSteps}, attempt ${attempt + 1}/${maxRecovery + 1} — asking ${aiConfig.model}`,
      );
      const raw = await requestAiChat(
        aiConfig,
        buildMessages(prompt, core, failureText),
      );
      envelopes.push(raw);

      try {
        const envelope = parseAiCommandEnvelope(raw);
        const batch = prepareAiCommandBatchForState(
          envelope.commands,
          envelope.continue,
          core.latestDocument(),
          core.latestViewport(),
        );
        const failure = await executeBatch(core, batch);
        if (failure) {
          failureText = failure;
          errors.push(failure);
          continue;
        }
        await refreshState(core);
        if (!batch.continue || step >= aiConfig.maxAgentSteps) {
          return makeResult(core, envelopes, errors, step);
        }
        completed = true;
        break;
      } catch (error) {
        failureText = formatAiCommandError(error);
        errors.push(failureText);
      }
    }

    if (!completed) {
      // Recovery budget exhausted for this step — report what we have.
      return makeResult(core, envelopes, errors, step);
    }
    step++;
  }
  return makeResult(core, envelopes, errors, step);
}
