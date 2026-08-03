import type { FiddleId } from "@canvas-wasm-fiddles/canvas-worker";

export type Fiddle = {
  id: FiddleId;
  title: string;
  summary: string;
  technique: string;
  color: string;
  initials: string;
};

export const fiddles = [
  {
    id: "ribbon-field",
    title: "Ribbon field",
    summary:
      "Animated Skia ribbons cut a horizontal, font-cycling “Hello” into flat saturated letter pieces with Path Ops.",
    technique: "Skia text paths · Path Ops · WebGL",
    color: "#ff8066",
    initials: "RF",
  },
  {
    id: "skia-webgl",
    title: "Skia Drawing (WebGL)",
    summary: "Eight groups of colorful, eye-tipped tentacles drawn with Skia Ganesh.",
    technique: "Skia Ganesh · WebGL 2",
    color: "#7de2ba",
    initials: "WG",
  },
  {
    id: "skia-cpu",
    title: "Skia Drawing (CPU)",
    summary: "The identical Skia tentacle scene rasterized on the CPU for comparison.",
    technique: "Skia Raster · CPU + putImageData",
    color: "#f2b5d4",
    initials: "CP",
  },
  {
    id: "elastic-text",
    title: "Elastic text",
    summary:
      "Skia Paragraph reflows a inside an elastic panel, demonstrating various text alignments, font loading.",
    technique: "Skia Paragraph · Ganesh WebGL",
    color: "#f6c453",
    initials: "ET",
  },
  {
    id: "sksl-image-proc",
    title: "SkSL Image Proc",
    summary:
      "SkSL (Skia shading Language) shader demo, cycles through permutations of the RGB channels of the input texture.",
    technique: "SkSL RuntimeEffect · Ganesh WebGL",
    color: "#ff91b9",
    initials: "IP",
  },
  {
    id: "shape-intersection",
    title: "Shape intersection",
    summary: "Skia Path Ops (intersection, difference) using geometries and tentacled blobs.",
    technique: "Skia Path Ops · mixed contours",
    color: "#b8e0d4",
    initials: "SI",
  },
  {
    id: "shape-tracing",
    title: "Shape tracing",
    summary:
      "A tangent arrow traces every contour of an outlined glyph while contour-relative markers expose path distance and direction.",
    technique: "SkPathMeasure · text outlines · WebGL",
    color: "#67d9e8",
    initials: "ST",
  },
  {
    id: "shape-morphing",
    title: "Shape Morphing",
    summary:
      "A Skia geometry engine synchronizes curve subdivision to continuously morph an outlined B into an S.",
    technique: "Canonical cubics · arc length · WebGL",
    color: "#a78bfa",
    initials: "SM",
  },
  {
    id: "vortex-field",
    title: "Vortex Field",
    summary:
      "Four counter-rotating local fields spiral filled compound shapes while following collision-aware coverage paths.",
    technique: "Catmull–Rom · Path Ops · WebGL",
    color: "#f47cb6",
    initials: "VF",
  },
] as const satisfies readonly Fiddle[];
