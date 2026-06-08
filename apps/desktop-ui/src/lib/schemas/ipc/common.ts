import { z } from "zod";

export const planeFrameSchema = z.object({
  origin: z.object({ x: z.number(), y: z.number(), z: z.number() }),
  x_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
  y_axis: z.object({ x: z.number(), y: z.number(), z: z.number() }),
  normal: z.object({ x: z.number(), y: z.number(), z: z.number() }),
});
