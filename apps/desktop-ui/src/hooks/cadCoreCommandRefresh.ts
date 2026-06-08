import {
  makeGetSessionStateCommand,
  makeGetViewportStateCommand,
  sendCoreCommand,
} from "@/lib";
import type { CoreCommand } from "@/types";

export async function sendAndRefreshViewport(command: CoreCommand) {
  await sendCoreCommand(command);
  await sendCoreCommand(makeGetViewportStateCommand());
}

export async function sendAndRefreshSessionViewport(command: CoreCommand) {
  await sendCoreCommand(command);
  await sendCoreCommand(makeGetSessionStateCommand());
  await sendCoreCommand(makeGetViewportStateCommand());
}
