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

Build the standalone Wasm demo separately:

```sh
cd cc-engine
./scripts/build-and-export.sh
```

Select **Skia WebGL pulse** in the app to run the native Skia Ganesh demo.
Skia wraps the WebGL canvas framebuffer as a GPU-backed `SkSurface` and draws
to it without a Wasm pixel buffer or a 2D-canvas upload.

Select **Skia CPU pulse** to run the exact same Skia draw function against a
raster `SkSurface`. Both variants report aligned `surface`, `scene-draw`, and
`present` timings every 120 frames.
