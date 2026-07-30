import type { FiddleId } from "@canvas-wasm-fiddles/canvas-worker";
import { CaseSensitive, Orbit, Shapes, Waves } from "lucide-svelte";

export type Fiddle = {
  id: FiddleId;
  number: string;
  title: string;
  summary: string;
  technique: string;
  color: string;
  icon: typeof Orbit;
};

export const fiddles = [
  {
    id: "orbital-bloom",
    number: "001",
    title: "Orbital bloom",
    summary: "A field of particles moving through four nested elliptical orbits.",
    technique: "OffscreenCanvas · particles",
    color: "#c7f36b",
    icon: Orbit,
  },
  {
    id: "ribbon-field",
    number: "002",
    title: "Ribbon field",
    summary: "Layered sine waves turn into a soft kinetic textile.",
    technique: "2D paths · oscillation",
    color: "#ff8066",
    icon: Waves,
  },
  {
    id: "skia-webgl",
    number: "003",
    title: "Skia Pulse (WebGL)",
    summary:
      "Animated field drawn with the Skia WebGL (Ganesh), directly into the canvas framebuffer.",
    technique: "Skia Ganesh · WebGL 2",
    color: "#7de2ba",
    icon: Shapes,
  },
  {
    id: "skia-cpu",
    number: "004",
    title: "Skia Pulse (CPU)",
    summary: "Identical Skia pulse scene, but rasterizes on the CPU, used for benchmark.",
    technique: "Skia Raster · CPU + putImageData",
    color: "#f2b5d4",
    icon: Shapes,
  },
  {
    id: "elastic-text",
    number: "005",
    title: "Elastic text",
    summary: "Skia Paragraph reflows a inside an elastic panel, demonstrating various text alignments, font loading.",
    technique: "Skia Paragraph · Ganesh WebGL",
    color: "#e0ff7a",
    icon: CaseSensitive,
  },
] as const satisfies readonly Fiddle[];
