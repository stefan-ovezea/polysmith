import { useTranslation } from "react-i18next";
import { getShowHiddenEdges, setShowHiddenEdges } from "@/utils/viewport/primitiveObjects";
import { useState } from "react";

export function ViewPanel() {
  const { t } = useTranslation();
  const [showHiddenEdges, setShowHiddenEdgesLocal] = useState(getShowHiddenEdges);

  function toggleHiddenEdges() {
    const next = !showHiddenEdges;
    setShowHiddenEdgesLocal(next);
    setShowHiddenEdges(next);
    // Trigger a re-render of the viewport so edges pick up the new material
    // settings. We bump a global revision counter that the viewport renderer
    // watches, causing all edge objects to be rebuilt with the updated flag.
    window.dispatchEvent(new CustomEvent("view-setting-changed"));
  }

  return (
    <section className="cad-floating-panel w-[280px] p-4">
      <p className="cad-kicker">{t("view.title")}</p>

      <div className="mt-3 space-y-3">
        <label className="flex items-center gap-3 cursor-pointer">
          <input
            type="checkbox"
            className="cad-checkbox"
            checked={showHiddenEdges}
            onChange={toggleHiddenEdges}
          />
          <div>
            <p className="text-sm text-on-surface">{t("view.showHiddenEdges")}</p>
            <p className="text-xs text-on-surface-dim">
              {t("view.showHiddenEdgesHint")}
            </p>
          </div>
        </label>
      </div>
    </section>
  );
}
