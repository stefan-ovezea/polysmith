// ESLint configuration for the desktop UI.
//
// The one rule that MUST stay an error is `react-hooks/rules-of-hooks`: a hook
// called conditionally, or after an early `return`, renders a different number of
// hooks between renders. React then throws "Rendered more hooks than during the
// previous render" and unmounts the whole UI — TypeScript and the unit tests
// stay green while the app is dead. That is not a style opinion, so it fails the
// build (see `pnpm lint:hooks`, which also self-checks that the rule fires).
//
// `exhaustive-deps` is a warning: the codebase already carries deliberate
// `eslint-disable react-hooks/exhaustive-deps` comments on effects whose
// dependency lists are intentionally partial (drag/animation state read through
// refs), so the rule is honest to have on, but it is a hint, not a gate.
import tseslintParser from "@typescript-eslint/parser";
import reactHooks from "eslint-plugin-react-hooks";

export default [
  {
    files: ["apps/desktop-ui/src/**/*.{ts,tsx}"],
    ignores: [
      "**/*.test.ts",
      "**/*.test.tsx",
      "**/node_modules/**",
      "apps/desktop-ui/src-tauri/**",
    ],
    linterOptions: {
      // Stale `eslint-disable` comments are pre-existing housekeeping, not a
      // signal anyone acts on; do not drown the run in them.
      reportUnusedDisableDirectives: "off",
    },
    languageOptions: {
      parser: tseslintParser,
      parserOptions: {
        ecmaVersion: "latest",
        sourceType: "module",
        ecmaFeatures: { jsx: true },
      },
    },
    plugins: {
      "react-hooks": reactHooks,
    },
    rules: {
      "react-hooks/rules-of-hooks": "error",
      "react-hooks/exhaustive-deps": "warn",
    },
  },
];
