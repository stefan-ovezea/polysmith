import { definePlugin } from "../sdk";
import { defaultGridfinityConfig } from "./defaultConfig";
import { GridfinitySettingsPanel } from "./GridfinitySettingsPanel";
import type { GridfinityPluginConfig } from "./types";

export const gridfinityPlugin = definePlugin<GridfinityPluginConfig>({
  manifest: {
    id: "polysmith.gridfinity",
    name: "Gridfinity Generator",
    version: "0.1.0",
    sdkVersion: "0.1",
    description: "Generate Gridfinity bins and baseplates as native CAD features.",
  },
  defaultConfig: defaultGridfinityConfig,
  activate() {
    return {
      menuItems: [
        {
          id: "open-gridfinity-generator",
          labelKey: "plugins.gridfinity.title",
          command: "gridfinity.open",
          disabledWhenCoreOffline: true,
        },
      ],
      renderSettings: (props) => <GridfinitySettingsPanel {...props} />,
    };
  },
});

export { GridfinityPanel } from "./GridfinityPanel";
export { configToFeatureParameters } from "./defaultConfig";
export type {
  GridfinityFeatureParameters,
  GridfinityPluginConfig,
} from "./types";
