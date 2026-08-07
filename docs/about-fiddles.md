# About the fiddles

Each fiddle isolates a particular Skia or vector-graphics capability. The
bookmarks below match the fiddle keys used by the application and native
registry.

<a id="text-reflow"></a>

## Text Reflow

Demonstrates Skia Paragraph shaping, line breaking, reflow, clipping, and
alignment inside a continuously resizable text frame.

<a id="text-cutting"></a>

## Text Cutting

Text shaping demo. Cut a text shape by undulating ribbons, stacked wavy bands.

<a id="text-tracing"></a>

## Text Tracing

Demonstrates measuring and traversing individual path contours in a letter. Follows distance-based positions and tangent directions.

<a id="text-morphing"></a>

## Text Morphing

Morphs from one letter shape ito another. Topology-aware interpolation between vector shapes after their
contours and segments are normalized into corresponding cubic paths.

<a id="env-distort"></a>

## Envelope Distort

Envelope warp on text shapes. Uses bicubic free-form deformation of vector text outlines
through 4 × 4 Bézier control patches.

<a id="mesh-warp"></a>

## Mesh Warp

Mesh warp, but using vector. Demonstrates local, topology-preserving vector deformation through a dense
control lattice and subdivided path geometry.

<a id="swirl-deform"></a>

## Swirl Deform

Inspired after the Illustrator wirl effect, demonstrates a localized circular vector warping through signed radial fields. Uses selective subdivision, and cubic reconstruction of affected path regions.

<a id="pucker-bloat"></a>

## Pucker and Bloat

Inspired by the popular Illustrator feature of the same name. It's a radial path deformation based on Béziers anchors and a pivot (center of a custom pivot).

<a id="shape-intersection"></a>

## Shape intersection

Demonstrates boolean path operations across various types of objects: curved, concave, and compound paths, all cutting each other into pieces.

<a id="contour-lines"></a>

## Contour Lines

Contour map based on a 2D field (e.g. Perlin noise). Computes layered vector
contours.

<a id="sksl-shader"></a>

## SkSL Shader

Demonstrates a simple fragment shader in SkSL (Skia shadig language). Performs color-channel swapping on an input image.

<a id="scene-benchmark-webgl"></a>

## Scene Benchmark (WebGL)

A Skia drawing scene rendered through a Ganesh (Skia WebGL backend) surface, backed
directly by a worker-owned WebGL framebuffer.

<a id="scene-benchmark-cpu"></a>

## Scene Benchmark (CPU)

Same Skia drawing scene rendered through a CPU raster surface, used for benchmarking.
