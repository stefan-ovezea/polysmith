import { definePlugin } from "../sdk";
import {
  configToFeatureParameters,
  defaultGridfinityConfig,
  migrateGridfinityConfig,
} from "./defaultConfig";
import { GridfinitySettingsPanel } from "./GridfinitySettingsPanel";
import { GridfinityPanel } from "./GridfinityPanel";
import { makeCreateGridfinityFeatureCommand } from "./commands";
import type {
  GridfinityFeatureParameters,
  GridfinityPluginConfig,
} from "./types";
import { GRIDFINITY_PLUGIN_ID } from "./types";

interface GridfinityActionState {
  featureId: string;
  parameters: GridfinityFeatureParameters;
}

const GRIDFINITY_OPEN_COMMAND = "gridfinity.open";
const GRIDFINITY_EDIT_ACTION = "gridfinity.edit";

export const gridfinityPlugin = definePlugin<GridfinityPluginConfig>({
  manifest: {
    id: "polysmith.gridfinity",
    name: "Gridfinity Generator",
    version: "0.1.0",
    sdkVersion: "0.1",
    description: "Generate Gridfinity bins and baseplates as native CAD features.",
  },
  defaultConfig: defaultGridfinityConfig,
  migrateConfig: migrateGridfinityConfig,
  activate(context) {
    return {
      menuItems: [
        {
          id: "open-gridfinity-generator",
          labelKey: "plugins.gridfinity.title",
          command: GRIDFINITY_OPEN_COMMAND,
          disabledWhenCoreOffline: true,
        },
      ],
      renderSettings: (props) => <GridfinitySettingsPanel {...props} />,
      handleCommand: async (command) => {
        if (command !== GRIDFINITY_OPEN_COMMAND) {
          return null;
        }

        const parameters = configToFeatureParameters(context.config);
        await context.sendCommand(makeCreateGridfinityFeatureCommand(parameters));
        const created = await context.awaitCreatedFeature(
          (feature) =>
            feature.kind === "plugin_feature" &&
            feature.plugin_feature_parameters?.plugin_id === GRIDFINITY_PLUGIN_ID,
        );
        await context.refreshViewport();
        return {
          actionId: GRIDFINITY_EDIT_ACTION,
          featureId: created.featureId,
          state: {
            featureId: created.featureId,
            parameters,
          } satisfies GridfinityActionState,
        };
      },
      renderAction: ({ action, disabled, onClose }) => {
        if (action.actionId !== GRIDFINITY_EDIT_ACTION) {
          return null;
        }
        const state = action.state as GridfinityActionState;
        return (
          <GridfinityPanel
            disabled={disabled}
            featureId={state.featureId}
            initialParameters={state.parameters}
            onClose={onClose}
          />
        );
      },
    };
  },
});

export { GridfinityPanel } from "./GridfinityPanel";
export { configToFeatureParameters } from "./defaultConfig";
export type {
  GridfinityFeatureParameters,
  GridfinityPluginConfig,
} from "./types";
