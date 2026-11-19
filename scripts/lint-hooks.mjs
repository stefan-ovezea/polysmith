// Lint the desktop UI for React hook misuse, and prove the gate can fail.
//
// Why this exists: a `useRef`/`useEffect` pair added below FeatureTimeline's
// `if (!document) return null;` rendered a different number of hooks between
// renders. React threw "Rendered more hooks than during the previous render" and
// unmounted the entire UI, while `tsc --noEmit` and all unit tests stayed green.
// `react-hooks/rules-of-hooks` catches exactly that, so it is an error here and
// `pnpm lint:hooks` is part of the lint gate.
//
// The second half of this script matters as much as the first: a lint gate that
// silently stops applying (a config typo, a moved source directory, a parser
// that no longer matches) is worse than none. So it lints a known-bad snippet and
// fails if the rule does NOT fire.
import { ESLint } from "eslint";

const eslint = new ESLint();

// 1) The real tree.
const results = await eslint.lintFiles(["apps/desktop-ui/src"]);
const errorFiles = results.filter((result) => result.errorCount > 0);
const errorCount = results.reduce(
  (total, result) => total + result.errorCount,
  0,
);
const warningCount = results.reduce(
  (total, result) => total + result.warningCount,
  0,
);

const formatter = await eslint.loadFormatter("stylish");
if (errorFiles.length > 0) {
  process.stdout.write(await formatter.format(errorFiles));
}
console.log(
  `hooks lint: ${results.length} file(s), ${errorCount} error(s), ${warningCount} warning(s)`,
);

// 2) Self-check: a hook after an early return must be reported.
const [selfCheck] = await eslint.lintText(
  [
    "export function Broken({ ready }: { ready: boolean }) {",
    "  if (!ready) {",
    "    return null;",
    "  }",
    "  const ref = useRef(null);",
    "  return <div ref={ref} />;",
    "}",
    "",
  ].join("\n"),
  { filePath: "apps/desktop-ui/src/__hooks_lint_self_check.tsx" },
);
const ruleFired = selfCheck.messages.some(
  (message) => message.ruleId === "react-hooks/rules-of-hooks",
);
if (!ruleFired) {
  console.error(
    "hooks lint: FAILED — a hook after an early return was NOT reported. " +
      "The rules-of-hooks rule is not being applied; check eslint.config.mjs " +
      "and the files glob before trusting a green run.",
  );
  process.exit(1);
}
console.log("hooks lint: self-check ok (a hook after an early return is reported)");

if (errorCount > 0) {
  process.exit(1);
}
