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
    id: "skia-pulse",
    number: "003",
    title: "Skia pulse",
    summary: "Skia Graphite records an animated field directly into the WebGPU canvas texture.",
    technique: "Skia Graphite · WebGPU",
    color: "#7de2ba",
    icon: Shapes,
  },
] as const satisfies readonly Fiddle[];
