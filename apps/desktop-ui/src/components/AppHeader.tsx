const modes = ["Sketch", "Extrude", "Fillet", "Shell", "Inspect"];

export function AppHeader() {
  return (
    <header className="pointer-events-none absolute inset-x-0 top-0 z-20 flex items-start justify-between px-8 pb-4 pt-6">
      <div className="pointer-events-auto flex items-center gap-8">
        <div>
          <p className="font-display text-[2rem] font-bold uppercase tracking-[0.06em] text-primary-glow">
            PolySmith
          </p>
          <p className="mt-1 text-xs uppercase tracking-[0.25em] text-on-surface-dim">
            Precision Nebula
          </p>
        </div>
        <nav className="cad-panel-soft flex items-center gap-8 px-6 py-4">
          {modes.map((mode, index) => (
            <button
              key={mode}
              className={index === 0 ? "cad-tab cad-tab-active" : "cad-tab"}
            >
              {mode}
            </button>
          ))}
        </nav>
      </div>

      <div className="pointer-events-auto cad-panel-soft flex items-center gap-3 px-4 py-3">
        <div className="h-3 w-3 rounded-full bg-success shadow-[0_0_12px_rgba(43,217,120,0.45)]" />
        <span className="text-sm tracking-[0.08em] text-on-surface-muted">
          Local Session
        </span>
      </div>
    </header>
  );
}
