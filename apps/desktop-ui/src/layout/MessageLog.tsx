interface MessageLogProps {
  messages: string[];
}

export function MessageLog({ messages }: MessageLogProps) {
  return (
    <section className="pointer-events-auto cad-floating-panel min-h-0 px-4 py-4">
      <div className="flex items-center justify-between gap-4">
        <p className="cad-kicker">Core Messages</p>
        <span className="text-xs uppercase tracking-[0.18em] text-on-surface-dim">
          {messages.length} entries
        </span>
      </div>
      <pre className="cad-scrollbar mt-4 max-h-40 overflow-auto whitespace-pre-wrap text-xs leading-6 text-on-surface-muted">
        {messages.join("\n")}
      </pre>
    </section>
  );
}
