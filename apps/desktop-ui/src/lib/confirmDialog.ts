import { ask } from "@tauri-apps/plugin-dialog";

import { i18n } from "@/i18n";
import { makeUiLogEntry } from "@/lib/logger";
import { useCadCoreStore } from "@/state";

/**
 * Ask the user to confirm something, with a real answer.
 *
 * `window.confirm` cannot be used in this app. tauri-plugin-dialog installs an
 * init script that REPLACES it with `invoke("plugin:dialog|confirm")`, and
 * plugin-dialog 2.7 no longer registers a `confirm` command — its
 * `allow-confirm`/`allow-ask` permissions are documented aliases of `message` —
 * so every call failed with "dialog.confirm not allowed. Command not found" and
 * the rejection surfaced as an unhandled rejection.
 *
 * The worse half: that shim returns a Promise, and a Promise is always truthy, so
 * `if (window.confirm(message))` silently took the DESTRUCTIVE branch — deleting
 * features and discarding sketches without asking. `ask()` goes through the
 * `message` command, which does exist, and resolves to a real boolean.
 *
 * A dialog that cannot be shown counts as "no": a confirmation that could not
 * reach the user must never destroy anything.
 */
export async function confirmDialog(message: string): Promise<boolean> {
  try {
    return await ask(message, {
      kind: "warning",
      okLabel: i18n.t("common.confirm"),
      cancelLabel: i18n.t("common.cancel"),
    });
  } catch (error) {
    useCadCoreStore.getState().addLogEntry(
      makeUiLogEntry(
        "warn",
        "desktop_ui",
        `confirm dialog unavailable, treating it as cancel: ${String(error)}`,
      ),
    );
    return false;
  }
}
