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
  disabled?: boolean;
};

export const fiddles: readonly Fiddle[] = [
  {
    id: "text-reflow",
    title: "Text Reflow",
    summary:
      "Demonstrates Skia Paragraph shaping, line breaking, reflow, clipping, and alignment inside a continuously resizable text frame.",
    technique: "Skia Paragraph · Ganesh WebGL",
    color: "#f6c453",
    initials: "TR",
    tags: ["WebGL", "Skia Ganesh"],
  },
  {
    id: "text-on-cirve",
    title: "Text On Curve",
    summary:
      "Shapes a repeating line of text and adaptively bends its glyph outlines along an editable SVG path, with protection for sharp corners and high-curvature pinches.",
    technique: "HarfBuzz shaping · arc-length frames · adaptive subdivision",
    color: "#ff0a78",
    initials: "TO",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "text-cutting",
    title: "Text Cutting",
    summary: "Text shaping demo. Cut a text shape by undulating ribbons, stacked wavy bands.",
    technique: "Skia text paths · Path Ops · WebGL",
    color: "#ff8066",
    initials: "TC",
    tags: ["WebGL", "Skia Ganesh", "Path Ops"],
  },
  {
    id: "text-tracing",
    title: "Text Tracing",
    summary:
      "Demonstrates measuring and traversing individual path contours in a letter. Follows distance-based positions and tangent directions.",
    technique: "SkPathMeasure · text outlines",
    color: "#67d9e8",
    initials: "TT",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "text-morphing",
    title: "Text Morphing",
    summary:
      "Morphs from one letter shape ito another. Topology-aware interpolation between vector shapes after their contours and segments are normalized into corresponding cubic paths.",
    technique: "Canonical cubics · arc length",
    color: "#a78bfa",
    initials: "TM",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "curve-interpolate",
    title: "Curve Interpolate",
    summary:
      "Interpolates corresponding open curves, distributes them by arc length along a guide, and modulates their perpendicular width with a sinusoidal profile.",
    technique: "Curve correspondence · arc-length placement · WebGL",
    color: "#8b1e2d",
    initials: "CI",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "env-distort",
    title: "Envelope Distort",
    summary:
      "Envelope warp on text shapes. Uses bicubic free-form deformation of vector text outlines through 4 × 4 Bézier control patches.",
    technique: "Bicubic Bézier FFD · text outlines · WebGL",
    color: "#78d8c0",
    initials: "ED",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "mesh-warp",
    title: "Mesh Warp",
    summary:
      "Mesh warp, but using vector. Demonstrates local, topology-preserving vector deformation through a dense control lattice and subdivided path geometry.",
    technique: "Control lattice · Path Ops · WebGL",
    color: "#58d6c7",
    initials: "MW",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "swirl-deform",
    title: "Swirl Deform",
    summary:
      "Inspired after the Illustrator wirl effect, demonstrates a localized circular vector warping through signed radial fields. Uses selective subdivision, and cubic reconstruction of affected path regions.",
    technique: "Catmull-Rom · Path Ops",
    color: "#f47cb6",
    initials: "SD",
    tags: ["WebGL", "Skia Ganesh", "Path Ops", "Advanced"],
  },
  {
    id: "noise-deform",
    title: "Noise Deform",
    summary:
      "Animates a water-like 3D Perlin field across colored text, selectively subdividing and refracting only the letter regions intersecting the moving effect box.",
    technique: "3D Perlin noise · Path Ops · text outlines",
    color: "#20a6dd",
    initials: "ND",
    tags: ["WebGL", "Skia Ganesh", "Path Ops", "Advanced"],
  },
  {
    id: "pucker-bloat",
    title: "Pucker and Bloat",
    summary:
      "Inspired by the popular Illustrator feature of the same name. It's a radial path deformation based on Béziers anchors and a pivot (center of a custom pivot).",
    technique: "Split cubics · radial anchors · WebGL",
    color: "#5f8fe8",
    initials: "PB",
    tags: ["WebGL", "Skia Ganesh", "Advanced"],
  },
  {
    id: "shape-intersection",
    title: "Shape intersection",
    summary:
      "Demonstrates boolean path operations across various types of objects: curved, concave, and compound paths, all cutting each other into pieces.",
    technique: "Skia Path Ops · mixed contours",
    color: "#b8e0d4",
    initials: "SI",
    tags: ["CPU Render", "Path Ops", "Advanced"],
  },
  {
    id: "contour-lines",
    title: "Contour Lines",
    summary:
      "Contour map based on a 2D field (e.g. Perlin noise). Computes layered vector contours.",
    technique: "Inclusive regions · Path Ops · WebGL",
    color: "#ffb347",
    initials: "CL",
    tags: ["WebGL", "Skia Ganesh", "Path Ops", "Advanced"],
  },
  {
    id: "contour-composite",
    title: "Contour 2: Composite Field",
    summary:
      "Combines animated noise with a fixed radial clear zone and renders the resulting contour bands with reusable vector hatch patterns.",
    technique: "SkPicture hatching · composite field · WebGL",
    color: "#30afff",
    initials: "C2",
    tags: ["WebGL", "Skia Ganesh", "Path Ops", "Advanced"],
  },
  {
    id: "sksl-shader",
    title: "SkSL Shader",
    summary:
      "Demonstrates a simple fragment shader in SkSL (Skia shadig language). Performs color-channel swapping on an input image.",
    technique: "SkSL RuntimeEffect · Ganesh WebGL",
    color: "#ff91b9",
    initials: "SS",
    tags: ["WebGL", "Skia Ganesh", "SkSL shader"],
  },
  {
    id: "scene-benchmark-webgl",
    title: "Scene Benchmark (WebGL)",
    summary:
      "A Skia drawing scene rendered through a Ganesh (Skia WebGL backend) surface, backed directly by a worker-owned WebGL framebuffer.",
    technique: "Skia Ganesh · WebGL 2",
    color: "#7de2ba",
    initials: "BW",
    tags: ["WebGL", "Skia Ganesh", "Benchmark"],
  },
  {
    id: "scene-benchmark-cpu",
    title: "Scene Benchmark (CPU)",
    summary:
      "Same Skia drawing scene rendered through a CPU raster surface, used for benchmarking.",
    technique: "Skia Raster · CPU + putImageData",
    color: "#f2b5d4",
    initials: "BC",
    tags: ["CPU Render", "Benchmark"],
  },
] as const satisfies readonly Fiddle[];

export type FiddleId = (typeof fiddles)[number]["id"];
