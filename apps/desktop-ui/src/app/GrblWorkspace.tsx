import { useTranslation } from "react-i18next";

import { CamGrblPanel } from "../layout";

interface GrblWorkspaceProps {
  // File selected by the CAM "Send to GRBL workspace" handoff (P4), or
  // null when the user arrived via the workspace switcher. Stream sends
  // it directly; the toolpath preview consumes it in P3.
  embeddedFilePath: string | null;
}

// Standalone GRBL workspace page: fixed left column hosting the same
// GRBL machine panel the CAM setup entry uses (both coexist — the panel
// is rendered embedded here, floating there), right side reserved for
// the toolpath preview. No sidebar, no timeline, no CAM panels — the
// page template follows SlicerWorkspace.
export function GrblWorkspace({ embeddedFilePath }: GrblWorkspaceProps) {
  const { t } = useTranslation();

  return (
    <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
      <div className="flex items-center gap-2 border-b border-[var(--cad-panel-soft-border)] px-3 py-1.5">
        <span className="cad-kicker">{t("workspace.grbl")}</span>
      </div>
      <div className="flex min-h-0 w-full min-w-0 flex-1">
        <aside className="min-h-0 w-[340px] shrink-0 border-r border-[var(--cad-panel-soft-border)] bg-surface-lowest">
          <CamGrblPanel
            embedded
            embeddedFilePath={embeddedFilePath}
            onClose={() => {}}
          />
        </aside>
        {/* Toolpath preview host — placeholder until P3. */}
        <div className="flex min-h-0 min-w-0 flex-1 items-center justify-center bg-surface-lowest">
          <span className="max-w-xl px-6 text-center text-sm text-on-surface-muted">
            {t("workspace.grblView")}
          </span>
        </div>
      </div>
    </section>
  );
}
