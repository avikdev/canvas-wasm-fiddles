# Canvas Wasm Fiddles

A Bun/Turbo workspace for small canvas experiments, backed by a standalone
C++/Bazel WebAssembly project.

## Workspace

- `fiddle-app` — Svelte 5 PWA with the fiddle browser.
- `canvas-worker` — TypeScript Web Worker that owns the canvas render loop.
- `cc-engine` — independent Bazel module that compiles a demo C++ API to Wasm.

```sh
bun install
bun run dev
```

## GitHub Pages

The production build uses `/canvas-wasm-fiddles/` as its Vite base path, so it
can coexist with other GitHub Pages projects at:

`https://avikdev.github.io/canvas-wasm-fiddles/`

Build and publish the app to the `gh-pages` branch:

```sh
bun install
bun run deploy
```

In the repository's GitHub settings, open **Pages**, choose **Deploy from a
branch**, and select the `gh-pages` branch with the `/ (root)` folder. The
deploy command builds the full workspace, adds `.nojekyll`, and publishes
`fiddle-app/dist` to that branch. Git must be authenticated for the `origin`
remote before running it.

## About the fiddles

### Contour Lines

This prototype turns a time-varying scalar field into editable vector
topography. Marching Squares extracts threshold crossings, shared grid-edge
identities stitch unordered segments into continuous contours, and
Catmull–Rom interpolation converts those polylines into cubic Bézier paths.
Filled isobands demonstrate how the same sampled field can partition a canvas
into regions suitable for maps, heat fields, scientific plots, and procedural
artwork.

### Ribbon field

This prototype validates the vector-text pipeline needed by a design editor:
shaping text with Skia Paragraph, extracting glyphs as multi-contour paths, and
preserving counters while applying boolean operations. It also demonstrates
turning a stroke into filled geometry and subtracting it from its source path
to create a true inset. Together, these techniques support editable text
outlines, knockouts, trims, and shape-aware spacing.

### Skia Drawing (WebGL)

This prototype validates Skia Ganesh as the editor's GPU canvas backend. A
worker-owned WebGL 2 framebuffer is wrapped directly in a GPU-backed
`SkSurface`, allowing animated vector paths, strokes, circles, antialiasing,
and alpha compositing without copying a Wasm pixel buffer into the browser.
Separate surface, drawing, flush, submit, and presentation timings provide a
baseline for profiling an interactive editor renderer.

### Skia Drawing (CPU)

This prototype runs the same scene-building code against a raster `SkSurface`
and presents the pixels through the browser's 2D canvas path. It verifies that
the editor's drawing model can remain independent of its rendering backend,
while providing a reference for GPU comparisons. The approach can support
fallback rendering, deterministic previews, headless export, and environments
where a reliable GPU context is unavailable.

### Elastic text

This prototype exercises the responsive text-frame behavior required in a
design editor. Skia Paragraph handles shaping, fallback and loaded font
families, inline styles, line breaking, clipping, and horizontal and vertical
alignment while its container continuously changes width. It demonstrates how
editable text can reflow predictably inside resizable frames and how alignment
controls can map onto the underlying layout engine.

### Envelope Distort

This prototype implements vector envelope warping as a bicubic Bézier
free-form deformation. Skia Paragraph glyph outlines are densely subdivided,
normalized into the patch's parameter domain, and evaluated through an animated
4 × 4 control lattice. Because the mapping operates on compound path contours
instead of a rasterized text image, counters remain holes and the result stays
editable vector geometry.

### SkSL Image Proc

This prototype validates SkSL runtime effects as a foundation for
non-destructive image adjustments. An image shader is supplied as a child to a
GPU runtime effect, while uniforms control channel remapping without rebuilding
the source image. The same pattern can power live color transforms, filters,
blend effects, and parameterized previews in an editor while keeping image
sampling and processing on the GPU.

### Shape intersection

This prototype stress-tests the geometry and compositing primitives behind
boolean shape tools. It applies Skia Path Ops to curved, concave, and
multi-contour paths, including even-odd holes, while retaining deterministic
identities for resulting regions. Drawing fills separately from clear-blended
outlines demonstrates erasing boundaries after composition. These capabilities
map directly to union, intersect, subtract, divide, compound-path, and
shape-builder workflows in a design editor.

### Shape tracing

This prototype demonstrates how a design editor can expose and traverse the
geometry of text outlines and compound shapes. It converts a glyph into a
multi-contour Skia path, measures each outer and inner boundary independently,
and derives positions and tangents at arbitrary distances. These capabilities
support path-direction inspection, animated motion paths, evenly distributed
anchors, contour-aware editing tools, and precise placement along vector
boundaries.

### Vortex Field

Each vortex defines a signed radial angle field. For a sample at distance
`d < radius`, the engine rotates it around the field center by up to three full
turns, multiplied by `1 - smoothstep(d / radius)`; the sign selects clockwise
or counter-clockwise rotation. Skia Path Ops first rejects contours outside
the field, intersecting fields are composed in sequence, and the displaced
samples are reconstructed as cubic Bézier contours with Catmull–Rom splines.
The source fill rule is retained so compound-path holes survive deformation.

Build the standalone Wasm demo separately:

```sh
./scripts/wasm-build-and-export
```

Select **Skia Drawing (WebGL)** in the app to run the native Skia Ganesh demo.
Skia wraps the WebGL canvas framebuffer as a GPU-backed `SkSurface` and draws
to it without a Wasm pixel buffer or a 2D-canvas upload.

Select **Skia Drawing (CPU)** to run the exact same Skia draw function against a
raster `SkSurface`. Both variants report aligned `surface`, `scene-draw`, and
`present` timings every 120 frames.
