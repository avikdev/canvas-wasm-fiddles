import type { FiddleId } from "@canvas-wasm-fiddles/canvas-worker";
import { Orbit, Shapes, Waves } from "lucide-svelte";

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
    title: "Skia WebGL pulse",
    summary: "Skia Ganesh draws an animated field directly into the WebGL canvas framebuffer.",
    technique: "Skia Ganesh · WebGL 2",
    color: "#7de2ba",
    icon: Shapes,
  },
  {
    id: "skia-cpu",
    number: "004",
    title: "Skia CPU pulse",
    summary:
      "The identical Skia pulse scene rasterizes on the CPU, then uploads its Wasm pixel buffer to the 2D canvas.",
    technique: "Skia Raster · CPU + putImageData",
    color: "#f2b5d4",
    icon: Shapes,
  },
] as const satisfies readonly Fiddle[];
