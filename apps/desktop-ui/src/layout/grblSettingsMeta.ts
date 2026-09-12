// Human-readable descriptions for common GRBL settings keys.
//
// Maps `$N` to an i18n key under `cam.grbl.settingsDesc.*` — the
// dialog renders the translated description next to each row.  Keys
// not listed here simply show no description (GRBL extensions vary by
// firmware).

export const GRBL_SETTINGS_DESC_KEYS: Record<string, string> = {
  "$0": "stepPulse",
  "$1": "stepIdleDelay",
  "$2": "stepPulseInvert",
  "$3": "directionInvert",
  "$4": "stepEnableInvert",
  "$5": "limitPinsInvert",
  "$6": "probeInvert",
  "$10": "statusReportMask",
  "$11": "junctionDeviation",
  "$12": "arcTolerance",
  "$13": "reportInches",
  "$20": "softLimits",
  "$21": "hardLimits",
  "$22": "homingCycle",
  "$23": "homingDirectionInvert",
  "$24": "homingFeed",
  "$25": "homingSeek",
  "$26": "homingDebounce",
  "$27": "homingPullOff",
  "$30": "maxSpindleSpeed",
  "$31": "minSpindleSpeed",
  "$32": "laserMode",
  "$100": "stepsX",
  "$101": "stepsY",
  "$102": "stepsZ",
  "$110": "maxRateX",
  "$111": "maxRateY",
  "$112": "maxRateZ",
  "$120": "accelX",
  "$121": "accelY",
  "$122": "accelZ",
  "$130": "maxTravelX",
  "$131": "maxTravelY",
  "$132": "maxTravelZ",
};
