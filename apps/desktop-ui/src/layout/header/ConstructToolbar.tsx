const constructTools = ["Offset Plane", "Midplane", "Axis", "Point"];

export function ConstructToolbar() {
  return (
    <>
      {constructTools.map((tool) => (
        <button key={tool} className="cad-tool-button" disabled>
          {tool}
        </button>
      ))}
    </>
  );
}
