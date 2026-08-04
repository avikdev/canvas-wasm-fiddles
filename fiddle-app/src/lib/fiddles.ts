export type FiddleTag =
  | "Advanced"
  | "Benchmark"
  | "CPU Render"
  | "Path Ops"
  | "Skia Ganesh"
  | "SkSL shader"
  | "WebGL";

export type Fiddle = {
  id: string;
  title: string;
  summary: string;
  technique: string;
  color: string;
  initials: string;
  tags: readonly FiddleTag[];
};

export const fiddles = [
  {
    id: "contour-lines",
    title: "Contour Lines",
    summary:
      "Marching Squares stitches animated 3D Perlin slices into smooth cubic contour boundaries and filled scalar bands.",
    technique: "Marching Squares · Catmull–Rom · WebGL",
    color: "#ffb347",
    initials: "CL",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "ribbon-field",
    title: "Ribbon field",
    summary:
      "Animated Skia ribbons cut a horizontal, font-cycling “Hello” into flat saturated letter pieces with Path Ops.",
    technique: "Skia text paths · Path Ops · WebGL",
    color: "#ff8066",
    initials: "RF",
    tags: ["WebGL", "Skia Ganesh", "Path Ops"],
  },
  {
    id: "skia-webgl",
    title: "Skia Drawing (WebGL)",
    summary: "Eight groups of colorful, eye-tipped tentacles drawn with Skia Ganesh.",
    technique: "Skia Ganesh · WebGL 2",
    color: "#7de2ba",
    initials: "WG",
    tags: ["WebGL", "Skia Ganesh", "Benchmark"],
  },
  {
    id: "skia-cpu",
    title: "Skia Drawing (CPU)",
    summary: "The identical Skia tentacle scene rasterized on the CPU for comparison.",
    technique: "Skia Raster · CPU + putImageData",
    color: "#f2b5d4",
    initials: "CP",
    tags: ["CPU Render", "Benchmark"],
  },
  {
    id: "elastic-text",
    title: "Elastic text",
    summary:
      "Skia Paragraph reflows a inside an elastic panel, demonstrating various text alignments, font loading.",
    technique: "Skia Paragraph · Ganesh WebGL",
    color: "#f6c453",
    initials: "ET",
    tags: ["WebGL", "Skia Ganesh"],
  },
  {
    id: "env-distort",
    title: "Envelope Distort",
    summary:
      "Dense Skia Paragraph outlines flow through animated bicubic Bézier envelopes while keeping every glyph fully vector.",
    technique: "Bicubic Bézier FFD · text outlines · WebGL",
    color: "#78d8c0",
    initials: "ED",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "sksl-image-proc",
    title: "SkSL Image Proc",
    summary:
      "SkSL (Skia shading Language) shader demo, cycles through permutations of the RGB channels of the input texture.",
    technique: "SkSL RuntimeEffect · Ganesh WebGL",
    color: "#ff91b9",
    initials: "IP",
    tags: ["WebGL", "Skia Ganesh", "SkSL shader"],
  },
  {
    id: "shape-intersection",
    title: "Shape intersection",
    summary: "Skia Path Ops (intersection, difference) using geometries and tentacled blobs.",
    technique: "Skia Path Ops · mixed contours",
    color: "#b8e0d4",
    initials: "SI",
    tags: ["CPU Render", "Path Ops", "Advanced"],
  },
  {
    id: "shape-tracing",
    title: "Shape tracing",
    summary:
      "A tangent arrow traces every contour of an outlined glyph while contour-relative markers expose path distance and direction.",
    technique: "SkPathMeasure · text outlines",
    color: "#67d9e8",
    initials: "ST",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "shape-morphing",
    title: "Shape Morphing",
    summary:
      "A Skia geometry engine synchronizes curve subdivision to continuously morph an outlined B into an S.",
    technique: "Canonical cubics · arc length",
    color: "#a78bfa",
    initials: "SM",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "vortex-field",
    title: "Vortex Field",
    summary:
      "Four counter-rotating local fields spiral filled compound shapes while following collision-aware coverage paths.",
    technique: "Catmull-Rom · Path Ops",
    color: "#f47cb6",
    initials: "VF",
    tags: ["WebGL", "Skia Ganesh", "Path Ops", "Advanced"],
  },
] as const satisfies readonly Fiddle[];

export type FiddleId = (typeof fiddles)[number]["id"];
