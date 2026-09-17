// Declarative renderer for the tool schematic built by
// lib/toolSchematic.ts: outline, flute/shank fills, center axis and
// the FL/OAL/Ø dimension lines.  The geometry comes in math space
// (y-up); one group transform flips and scales it uniformly into the
// SVG viewBox, so a Ø8×60 tool and a Ø1×38 tool read differently.
//
// Strokes use vector-effect="non-scaling-stroke" — line weight stays
// constant on screen no matter how far the profile is scaled down.

import { useMemo } from "react";

import {
  buildToolSchematic,
  type SchematicParams,
} from "@/lib/toolSchematic";
import type { ToolEntry } from "@/types";

const VIEW_WIDTH = 224;
const VIEW_HEIGHT = 288;
const MARGIN = 26;

function toParams(tool: ToolEntry): SchematicParams {
  return {
    type: tool.type,
    diameter_mm: tool.diameter_mm,
    shank_diameter_mm: tool.shank_diameter_mm,
    flute_length_mm: tool.flute_length_mm,
    shoulder_length_mm: tool.shoulder_length_mm,
    overall_length_mm: tool.overall_length_mm,
    corner_radius_mm: tool.corner_radius_mm,
    point_angle_deg: tool.point_angle_deg,
    tip_diameter_mm: tool.tip_diameter_mm,
    tip_length_mm: tool.tip_length_mm,
    taper_angle_deg: tool.taper_angle_deg,
  };
}

export function ToolSchematicView({
  tool,
  className,
}: {
  tool: ToolEntry;
  className?: string;
}) {
  const schematic = useMemo(() => buildToolSchematic(toParams(tool)), [tool]);

  // Uniform fit: the profile spans ±extent.width/2 horizontally and
  // extent.height vertically; keep it inside the view with margins.
  const scale = Math.min(
    (VIEW_WIDTH - 2 * MARGIN) / schematic.extent.width,
    (VIEW_HEIGHT - 2 * MARGIN) / schematic.extent.height,
  );
  const centerX = VIEW_WIDTH / 2;
  // Math y = overall_length maps to the top margin; y = lowestY maps
  // to the bottom margin.  SVG's y-down flip comes from the negative
  // scale in the group transform.
  const topY = MARGIN + scale * (tool.overall_length_mm || 1);
  const toScreen = (x: number, y: number): [number, number] => [
    centerX + scale * x,
    topY - scale * y,
  ];

  const axisStart = toScreen(0, schematic.extent.lowestY);
  const axisEnd = toScreen(0, tool.overall_length_mm || 1);

  return (
    <svg
      viewBox={`0 0 ${VIEW_WIDTH} ${VIEW_HEIGHT}`}
      className={className}
      role="img"
      aria-label={tool.name}
    >
      {/* Cutting + body fills and the outline share the math-space
          group so the regions stay glued to the profile. */}
      <g transform={`translate(${centerX} ${topY}) scale(${scale} ${-scale})`}>
        <path
          d={schematic.fluteRegion}
          fill="var(--color-primary-soft)"
          opacity={0.55}
        />
        <path
          d={schematic.shankRegion}
          fill="var(--cad-panel-soft-bg)"
        />
        <path
          d={schematic.outline}
          fill="none"
          stroke="var(--color-on-surface)"
          strokeWidth={1.25}
          vectorEffect="non-scaling-stroke"
          strokeLinejoin="round"
        />
      </g>

      {/* Center axis — dashed, drawn in screen space for a constant
          dash pattern at any scale. */}
      <line
        x1={axisStart[0]}
        y1={axisStart[1]}
        x2={axisEnd[0]}
        y2={axisEnd[1]}
        stroke="var(--cad-sketch-grid-center-axis)"
        strokeWidth={1}
        strokeDasharray="4 3"
      />

      {/* Dimension lines with end ticks; labels sit outside the
          measured span, upright in screen space. */}
      {schematic.dims.map((dim) => {
        const [x1, y1] = toScreen(dim.from[0], dim.from[1]);
        const [x2, y2] = toScreen(dim.to[0], dim.to[1]);
        const horizontal = Math.abs(y2 - y1) < Math.abs(x2 - x1);
        const tick = 3;
        const [tx1, ty1] = horizontal
          ? [x1, y1 - tick]
          : [x1 - tick, y1];
        const [tx2, ty2] = horizontal
          ? [x2, y2 - tick]
          : [x2 - tick, y2];
        const midX = (x1 + x2) / 2;
        const midY = (y1 + y2) / 2;
        const labelX = horizontal
          ? midX
          : dim.side === "left"
            ? midX - 6
            : midX + 6;
        const labelY = horizontal ? midY - 5 : midY + 3;
        return (
          <g key={dim.label}>
            <line
              x1={x1}
              y1={y1}
              x2={x2}
              y2={y2}
              stroke="var(--cad-sketch-dimension-label)"
              strokeWidth={1}
              strokeDasharray="2 2"
            />
            <line
              x1={tx1}
              y1={ty1}
              x2={tx2}
              y2={ty2}
              stroke="var(--cad-sketch-dimension-label)"
              strokeWidth={1}
            />
            <text
              x={labelX}
              y={labelY}
              fontSize={9}
              fill="var(--cad-sketch-dimension-label)"
              textAnchor={horizontal ? "middle" : dim.side === "left" ? "end" : "start"}
            >
              {dim.label}
            </text>
          </g>
        );
      })}
    </svg>
  );
}
