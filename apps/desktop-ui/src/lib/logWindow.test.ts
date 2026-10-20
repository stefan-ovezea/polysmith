import { describe, expect, it } from "vitest";

import { appendLogWithinWindow } from "./logWindow";
import type { LogEntry } from "@/types";

function entry(level: LogEntry["level"], message: string): LogEntry {
  return { level, message, source: "test", timestamp: "2026-10-19T00:00:00Z" };
}

describe("appendLogWithinWindow", () => {
  it("appends while there is room", () => {
    const logs = [entry("info", "a"), entry("warn", "b")];
    const next = appendLogWithinWindow(logs, entry("info", "c"), 10);
    expect(next.map((item) => item.message)).toEqual(["a", "b", "c"]);
  });

  it("evicts the oldest chatter before any warning", () => {
    // Window of 4, full of chatter, then a warning arrives: the oldest INFO
    // goes, not the warning, and the warning stays at the end.
    const logs = [
      entry("info", "one"),
      entry("info", "two"),
      entry("info", "three"),
      entry("info", "four"),
    ];
    const next = appendLogWithinWindow(logs, entry("error", "boom"), 4);
    expect(next.map((item) => item.message)).toEqual([
      "two",
      "three",
      "four",
      "boom",
    ]);
  });

  it("keeps every warning and error while any chatter remains", () => {
    const logs = [
      entry("error", "first failure"),
      entry("info", "chatter 1"),
      entry("warn", "second warning"),
      entry("info", "chatter 2"),
    ];
    const next = appendLogWithinWindow(logs, entry("info", "chatter 3"), 4);
    const messages = next.map((item) => item.message);
    expect(messages).toContain("first failure");
    expect(messages).toContain("second warning");
    expect(messages).not.toContain("chatter 1");
  });

  it("evicts the oldest diagnostic only when nothing else is left", () => {
    const logs = [
      entry("warn", "old warning"),
      entry("error", "old error"),
      entry("error", "recent error"),
    ];
    const next = appendLogWithinWindow(logs, entry("warn", "new warning"), 3);
    expect(next.map((item) => item.message)).toEqual([
      "old error",
      "recent error",
      "new warning",
    ]);
  });

  it("never grows past the window size", () => {
    let logs: LogEntry[] = [];
    for (let i = 0; i < 50; i += 1) {
      logs = appendLogWithinWindow(logs, entry("debug", `line ${i}`), 10);
    }
    expect(logs).toHaveLength(10);
    expect(logs[logs.length - 1].message).toBe("line 49");
  });
});
