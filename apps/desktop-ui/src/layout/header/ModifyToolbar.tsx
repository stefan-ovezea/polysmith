const modifyTools = ["Press Pull", "Fillet", "Shell", "Move"];

export function ModifyToolbar() {
  return (
    <>
      {modifyTools.map((tool) => (
        <button key={tool} className="cad-tool-button" disabled>
          {tool}
        </button>
      ))}
    </>
  );
}
