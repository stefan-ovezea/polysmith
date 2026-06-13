import { makeUiLogEntry, writeLogToConsole } from "@/lib";
import { useToastStore } from "@/state";

interface CoreLogReporter {
  addLogEntry: (entry: ReturnType<typeof makeUiLogEntry>) => void;
  addMessage: (message: string) => void;
  setStatus: (status: "error") => void;
}

export function reportCoreError(
  reporter: CoreLogReporter,
  source: string,
  message: string,
) {
  const entry = makeUiLogEntry("error", source, message);
  writeLogToConsole(entry);
  reporter.addLogEntry(entry);
  reporter.addMessage(entry.message);
  useToastStore.getState().pushToast("error", message);
  reporter.setStatus("error");
}
