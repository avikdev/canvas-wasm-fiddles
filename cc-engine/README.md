# C++ canvas engine

This is a standalone Bazel module. It builds a small Emscripten ES module with
Embind exports and has no dependency on the Bun workspace in the parent folder.

```sh
bazel build //:demo_wasm
```

The generated JavaScript loader and Wasm binary are written to:

```text
bazel-bin/demo_wasm/demo.js
bazel-bin/demo_wasm/demo.wasm
```

The loader exports an async `CreateCanvasDemoModule` factory. The initialized
module exposes `FiddleManager`, which owns the OffscreenCanvas, fiddle
registry, active fiddle, and animation time.

See [SKIA_CANVAS_WIRING.md](SKIA_CANVAS_WIRING.md) for the native Skia raster
surface, Wasm pixel-memory, OffscreenCanvas presentation path, and the boundary
for a future Graphite/WebGPU backend.

To build and export the JavaScript loader and Wasm binary into the worker
package:

```sh
./scripts/build-and-export.sh
```
