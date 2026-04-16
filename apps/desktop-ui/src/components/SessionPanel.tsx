import type { SessionState } from "../types/ipc";

interface SessionPanelProps {
  session: SessionState | null;
}

export function SessionPanel({ session }: SessionPanelProps) {
  if (!session) {
    return (
      <section className="cad-panel-soft px-4 py-4">
        <p className="cad-kicker">Session</p>
        <p className="mt-3 text-sm text-on-surface-muted">
          No session snapshot loaded.
        </p>
      </section>
    );
  }

  return (
    <section className="cad-panel-soft px-4 py-4">
      <p className="cad-kicker">Session</p>
      <div className="mt-4 grid grid-cols-2 gap-3">
        <div>
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            Documents
          </p>
          <p className="cad-metric mt-2">{session.document_count}</p>
        </div>
        <div>
          <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
            Active
          </p>
          <p className="cad-metric mt-2">
            {session.active_document_id ?? "None"}
          </p>
        </div>
      </div>
      <div className="mt-4 flex gap-2 text-xs uppercase tracking-[0.14em]">
        <span
          className={`rounded-full px-2 py-1 ${
            session.can_undo ? "bg-[rgba(17,80,90,0.55)] text-primary-soft" : "bg-[rgba(255,255,255,0.04)] text-on-surface-dim"
          }`}
        >
          Undo {session.can_undo ? "Ready" : "Empty"}
        </span>
        <span
          className={`rounded-full px-2 py-1 ${
            session.can_redo ? "bg-[rgba(17,80,90,0.55)] text-primary-soft" : "bg-[rgba(255,255,255,0.04)] text-on-surface-dim"
          }`}
        >
          Redo {session.can_redo ? "Ready" : "Empty"}
        </span>
      </div>
    </section>
  );
}
