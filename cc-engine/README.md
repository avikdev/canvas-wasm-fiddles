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

See [SCENE_BENCHMARK_WEBGL_WIRING.md](SCENE_BENCHMARK_WEBGL_WIRING.md) for the native Skia
Ganesh/WebGL context, the GPU-backed on-screen `SkSurface`, and the exact
zero-CPU-copy boundary.

See [SCENE_BENCHMARK.md](SCENE_BENCHMARK.md) for the matched
WebGL/CPU benchmark, timing semantics, and the source-level inspection of the
Ganesh flush and submit path.

To build and export the JavaScript loader and Wasm binary into the worker
package:

```sh
./scripts/build-and-export.sh
```
