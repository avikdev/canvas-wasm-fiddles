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

Build the standalone Wasm demo separately:

```sh
cd cc-engine
./scripts/build-and-export.sh
```

Select **Skia Drawing (WebGL)** in the app to run the native Skia Ganesh demo.
Skia wraps the WebGL canvas framebuffer as a GPU-backed `SkSurface` and draws
to it without a Wasm pixel buffer or a 2D-canvas upload.

Select **Skia Drawing (CPU)** to run the exact same Skia draw function against a
raster `SkSurface`. Both variants report aligned `surface`, `scene-draw`, and
`present` timings every 120 frames.
