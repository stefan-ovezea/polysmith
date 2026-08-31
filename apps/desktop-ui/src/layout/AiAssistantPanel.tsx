import { useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import type { AiConfig } from "@/config";
import {
  buildAiCadRecoveryPrompt,
  buildAiCadSystemPrompt,
  buildAiCadUserPrompt,
  buildAiWorkingReferences,
  commandPreviewLabel,
  formatAiCommandError,
  type AiExecutableCommand,
  makeGetSessionStateCommand,
  makeGetViewportStateCommand,
  parseAiCommandEnvelope,
  prepareAiCommandBatchForState,
  requestAiChat,
  sendCoreCommand,
} from "@/lib";
import { useCadCoreStore } from "@/state";
import type { CoreCommand, CoreMessage, DocumentState, ViewportState } from "@/types";

interface AiAssistantPanelProps {
  config: AiConfig;
  status: string;
  document: DocumentState | null;
  viewport: ViewportState | null;
  onClose: () => void;
  onStartCore: () => Promise<void>;
}

interface ChatEntry {
  role: "user" | "assistant" | "system";
  text: string;
}

interface PendingBatch {
  message: string;
  commands: AiExecutableCommand[];
  continue: boolean;
  step: number;
}

// How many times a rejected or failed batch is fed back to the model for a
// self-correction before the turn is abandoned. Bounded so a confused model
// cannot loop forever.
const MAX_RECOVERY_ATTEMPTS = 3;
// Multi-turn memory size: replay at most this many raw user prompts and raw
// model envelopes as prior turns. Historical state summaries are never kept
// (their IDs go stale); the fresh per-turn summary is the real context.
const MAX_HISTORY_TURNS = 6;

function waitForCoreResponse(commandId: string, timeoutMs = 6000) {
  return new Promise<CoreMessage>((resolve, reject) => {
    const timer = window.setTimeout(() => {
      unsubscribe();
      reject(new Error(`Timed out waiting for core response to ${commandId}`));
    }, timeoutMs);

    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const event = state.lastEvent;
      if (!event || event.id !== commandId) {
        return;
      }
      window.clearTimeout(timer);
      unsubscribe();
      if (event.type === "error") {
        reject(new Error(event.payload.message));
        return;
      }
      resolve(event);
    });
  });
}

async function sendAndWait(command: CoreCommand & { id: string }) {
  const response = waitForCoreResponse(command.id);
  await sendCoreCommand(command);
  await response;
}

async function refreshCoreSnapshot() {
  await sendAndWait(makeGetSessionStateCommand() as CoreCommand & { id: string });
  await sendAndWait(makeGetViewportStateCommand() as CoreCommand & { id: string });
}

export function AiAssistantPanel({
  config,
  status,
  document,
  viewport,
  onClose,
  onStartCore,
}: AiAssistantPanelProps) {
  const { t } = useTranslation();
  const [prompt, setPrompt] = useState("");
  const [activePrompt, setActivePrompt] = useState("");
  const [entries, setEntries] = useState<ChatEntry[]>([]);
  const [pendingBatch, setPendingBatch] = useState<PendingBatch | null>(null);
  const [isThinking, setIsThinking] = useState(false);
  const [isExecuting, setIsExecuting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const canAsk =
    !isThinking &&
    !isExecuting &&
    !pendingBatch &&
    config.model.trim().length > 0 &&
    config.baseUrl.trim().length > 0;
  const workingReferences = buildAiWorkingReferences(document, viewport);
  // Multi-turn memory for the model: raw user prompts and raw model envelopes
  // only (a ref avoids stale-closure reads across the async agent loop).
  const historyRef = useRef<Array<{ role: "user" | "assistant"; content: string }>>(
    [],
  );
  // Recovery attempts used for the current user turn. Reset when a new prompt
  // is submitted and after a batch executes successfully.
  const failureCountRef = useRef(0);
  // Stop signal for the agent loop: checked after every await so a runaway
  // model session can always be interrupted.
  const stopRequestedRef = useRef(false);

  function stopActiveWork() {
    stopRequestedRef.current = true;
    setIsThinking(false);
    setIsExecuting(false);
    setPendingBatch(null);
    setEntries((current) => [
      ...current,
      { role: "system", text: t("aiAssistant.stopped") },
    ]);
  }

  async function requestNextBatch(
    userPrompt: string,
    step: number,
    failureText?: string,
  ) {
    if (stopRequestedRef.current) {
      return;
    }
    setIsThinking(true);
    setError(null);
    try {
      const snapshot = useCadCoreStore.getState();
      const messages: Array<{ role: "system" | "user" | "assistant"; content: string }> = [
        { role: "system", content: buildAiCadSystemPrompt() },
        ...historyRef.current,
        {
          role: "user",
          content: buildAiCadUserPrompt(
            userPrompt,
            snapshot.document ?? document,
            snapshot.viewport ?? viewport,
          ),
        },
      ];
      if (failureText) {
        messages.push({
          role: "user",
          content: buildAiCadRecoveryPrompt(failureText),
        });
      }
      const response = await requestAiChat(config, messages);
      if (stopRequestedRef.current) {
        return;
      }
      // Record the turn for multi-turn memory: the raw prompt (or the recovery
      // failure text) plus the exact model envelope. Never store state
      // summaries here — their IDs go stale between turns.
      historyRef.current.push(
        {
          role: "user",
          content: failureText
            ? buildAiCadRecoveryPrompt(failureText)
            : userPrompt,
        },
        { role: "assistant", content: response },
      );
      while (historyRef.current.length > MAX_HISTORY_TURNS * 2) {
        historyRef.current.shift();
      }

      const envelope = parseAiCommandEnvelope(response);
      const preparedBatch = prepareAiCommandBatchForState(
        envelope.commands,
        envelope.continue,
        snapshot.document,
        snapshot.viewport,
      );
      setEntries((current) => [
        ...current,
        { role: "assistant", text: envelope.message },
        ...preparedBatch.notices.map((notice) => ({
          role: "system" as const,
          text: notice,
        })),
      ]);
      const batch: PendingBatch = {
        message: envelope.message,
        commands: preparedBatch.commands,
        continue: preparedBatch.continue,
        step,
      };
      if (!config.previewBeforeRun) {
        // Auto-run: execute immediately through the same machinery the
        // manual Run button uses (including recovery and continue loops).
        setIsExecuting(true);
        try {
          await executeBatch(batch);
        } finally {
          setIsExecuting(false);
        }
      } else {
        setPendingBatch(batch);
      }
    } catch (caught) {
      if (stopRequestedRef.current) {
        return;
      }
      if (failureCountRef.current < MAX_RECOVERY_ATTEMPTS) {
        failureCountRef.current += 1;
        setEntries((current) => [
          ...current,
          { role: "system", text: t("aiAssistant.recovering") },
        ]);
        await requestNextBatch(userPrompt, step, formatAiCommandError(caught));
      } else {
        setError(formatAiCommandError(caught));
      }
    } finally {
      setIsThinking(false);
    }
  }

  async function submitPrompt() {
    const nextPrompt = prompt.trim();
    if (!nextPrompt || !canAsk) {
      return;
    }
    failureCountRef.current = 0;
    stopRequestedRef.current = false;
    setActivePrompt(nextPrompt);
    setPrompt("");
    setEntries((current) => [...current, { role: "user", text: nextPrompt }]);
    await requestNextBatch(nextPrompt, 1);
  }

  // One recovery pass after a rejected or failed batch: refresh the snapshot
  // so the model sees the partial document, then ask for a corrected envelope.
  async function recoverFromFailure(
    batch: PendingBatch,
    failedCommand: AiExecutableCommand,
    caught: unknown,
  ) {
    try {
      await refreshCoreSnapshot();
    } catch {
      // Best-effort: the model can still read the pre-failure state summary.
    }
    if (failureCountRef.current < MAX_RECOVERY_ATTEMPTS) {
      failureCountRef.current += 1;
      setEntries((current) => [
        ...current,
        { role: "system", text: t("aiAssistant.recovering") },
      ]);
      await requestNextBatch(
        activePrompt,
        batch.step,
        `${commandPreviewLabel(failedCommand)}: ${formatAiCommandError(caught)}`,
      );
    } else {
      setError(formatAiCommandError(caught));
    }
  }

  async function executeBatch(batch: PendingBatch) {
    for (const command of batch.commands) {
      if (stopRequestedRef.current) {
        return;
      }
      try {
        await sendAndWait(command);
      } catch (caught) {
        if (stopRequestedRef.current) {
          return;
        }
        await recoverFromFailure(batch, command, caught);
        return;
      }
    }
    await refreshCoreSnapshot();
    setEntries((current) => [
      ...current,
      {
        role: "system",
        text: t("aiAssistant.executedCommands", {
          count: batch.commands.length,
          plural: batch.commands.length === 1 ? "" : "s",
        }),
      },
    ]);
    // A fully executed batch proves the model's last envelope was good.
    failureCountRef.current = 0;
    const shouldContinue = batch.continue && batch.step < config.maxAgentSteps;
    const nextStep = batch.step + 1;
    if (shouldContinue) {
      await requestNextBatch(activePrompt, nextStep);
    }
  }

  async function runPendingBatch() {
    if (!pendingBatch) {
      return;
    }
    const batch = pendingBatch;
    setIsExecuting(true);
    setError(null);
    setPendingBatch(null);
    try {
      await executeBatch(batch);
    } finally {
      setIsExecuting(false);
    }
  }

  function handlePromptKeyDown(event: React.KeyboardEvent<HTMLTextAreaElement>) {
    if (event.key !== "Enter" || event.shiftKey || event.nativeEvent.isComposing) {
      return;
    }
    event.preventDefault();
    void submitPrompt();
  }

  return (
    <aside className="cad-panel-soft flex w-[min(420px,42vw)] min-w-[360px] flex-col !rounded-none border-l border-white/10">
      <header className="flex items-center justify-between gap-3 border-b border-white/10 px-4 py-3">
        <div>
          <p className="cad-kicker">{t("aiAssistant.title")}</p>
          <p className="mt-1 text-xs text-on-surface-muted">
            {config.provider === "deepseek" ? "DeepSeek" : "Ollama"} ·{" "}
            {config.model.trim() || t("aiAssistant.noModel")}
          </p>
        </div>
        <button
          type="button"
          className="cad-ribbon-action"
          onClick={onClose}
        >
          {t("common.close")}
        </button>
      </header>

      <div className="cad-scrollbar min-h-0 flex-1 space-y-3 overflow-auto px-4 py-4">
        {status !== "connected" ? (
          <div className="rounded-md border border-white/10 bg-white/[0.025] px-3 py-3">
            <p className="text-sm text-on-surface">
              {t("aiAssistant.coreNotRunning")}
            </p>
            <button
              type="button"
              className="cad-ribbon-action mt-3"
              onClick={() => void onStartCore()}
            >
              {t("header.startCore")}
            </button>
          </div>
        ) : null}

        {entries.length === 0 ? (
          <p className="text-sm leading-5 text-on-surface-muted">
            {t("aiAssistant.intro")}
          </p>
        ) : null}

        <section className="rounded-md border border-white/10 bg-black/15 px-3 py-3">
          <p className="cad-kicker">{t("aiAssistant.workingReferences")}</p>
          <div className="mt-2 space-y-1 font-mono text-[0.7rem] leading-4 text-on-surface-muted">
            {workingReferences.map((reference) => (
              <p key={reference} className="break-words">
                {reference}
              </p>
            ))}
          </div>
        </section>

        {entries.map((entry, index) => (
          <div
            key={`${entry.role}-${index}`}
            className={
              entry.role === "user"
                ? "rounded-md bg-primary/15 px-3 py-2 text-sm text-on-surface"
                : entry.role === "assistant"
                  ? "rounded-md bg-white/[0.04] px-3 py-2 text-sm text-on-surface"
                  : "rounded-md border border-white/10 px-3 py-2 text-xs text-on-surface-muted"
            }
          >
            {entry.text}
          </div>
        ))}

        {pendingBatch ? (
          <section className="rounded-md border border-primary-edge/40 bg-white/[0.025] px-3 py-3">
            <p className="cad-kicker">{t("aiAssistant.commandPreview")}</p>
            <div className="mt-3 max-h-60 space-y-2 overflow-auto">
              {pendingBatch.commands.map((command) => (
                <pre
                  key={command.id}
                  className="overflow-auto rounded-md bg-black/25 px-3 py-2 text-xs text-on-surface-muted"
                >
                  {commandPreviewLabel(command)}
                </pre>
              ))}
            </div>
            <div className="mt-3 flex items-center gap-2">
              <button
                type="button"
                className="cad-ribbon-action cad-ribbon-action-primary"
                disabled={isExecuting || status !== "connected"}
                onClick={() => void runPendingBatch()}
              >
                {isExecuting ? t("aiAssistant.running") : t("aiAssistant.runCommands")}
              </button>
              <button
                type="button"
                className="cad-ribbon-action"
                disabled={isExecuting}
                onClick={() => setPendingBatch(null)}
              >
                {t("common.cancel")}
              </button>
            </div>
          </section>
        ) : null}

        {isThinking ? (
          <p className="text-sm text-on-surface-muted">
            {t("aiAssistant.waiting")}
          </p>
        ) : null}
        {error ? (
          <p className="rounded-md border border-red-400/30 bg-red-500/10 px-3 py-2 text-sm text-red-200">
            {error}
          </p>
        ) : null}
      </div>

      <footer className="border-t border-white/10 px-4 py-3">
        <textarea
          className="min-h-24 w-full resize-none rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
          value={prompt}
          placeholder={t("aiAssistant.placeholder")}
          disabled={Boolean(pendingBatch) || isThinking || isExecuting}
          onChange={(event) => setPrompt(event.target.value)}
          onKeyDown={handlePromptKeyDown}
        />
        <div className="mt-3 flex items-center justify-between gap-3">
          <span className="text-xs text-on-surface-muted">
            {config.enabled
              ? config.previewBeforeRun
                ? t("aiAssistant.previewBeforeRun")
                : t("aiAssistant.autoRunActive")
              : t("aiAssistant.disabled")}
          </span>
          {isThinking || isExecuting ? (
            <button
              type="button"
              className="cad-ribbon-action cad-ribbon-action-primary"
              onClick={stopActiveWork}
            >
              {t("aiAssistant.stop")}
            </button>
          ) : (
            <button
              type="button"
              className="cad-ribbon-action cad-ribbon-action-primary"
              disabled={!canAsk || status !== "connected"}
              onClick={() => void submitPrompt()}
            >
              {t("aiAssistant.send")}
            </button>
          )}
        </div>
      </footer>
    </aside>
  );
}
