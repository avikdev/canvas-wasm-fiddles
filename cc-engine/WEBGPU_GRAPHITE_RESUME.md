# WebGPU/Graphite implementation checkpoint

This note captures the completed investigation and the exact state of the
native Skia WebGPU work so a future pass can resume without repeating it.

## Goal and current status

The intended frame path is implemented in source:

```text
worker OffscreenCanvas
  -> GPUCanvasContext.configure()
  -> getCurrentTexture() each frame
  -> Emscripten imports the browser GPUTexture
  -> Skia Graphite wraps it as a BackendTexture and SkSurface
  -> SkCanvas records drawing
  -> Graphite submits directly to the canvas texture
```

This is the desired zero-CPU-copy path. There is no `readPixels`,
`ImageData`, 2D staging canvas, or `drawImage`.

The implementation has **not yet completed a full Wasm link or browser
rendering test**. The first full build reached Skia's Graphite/Dawn sources and
stopped on a missing Bazel header dependency. The narrow dependency fix is now
in `third_party/skia_graphite_dawn.patch`, but the build was not rerun after
that final edit because this work session was intentionally wrapped up.

## Important decisions already validated

- Keep the existing Emscripten toolchain at `4.0.17`.
- Do not vendor a second Emdawn package. Emscripten's built-in WebGPU headers,
  import helpers, and JS runtime are used.
- Use Skia commit `c5c3399b3a8483f04f6d302630de2402e59b5b46`
  (2025-11-14). Newer 2026 Skia revisions expect WebGPU enum/API additions not
  present in Emscripten 4.0.17.
- Skia's Bazel files at that commit do not publish a Graphite/Dawn target. The
  local patch exposes a focused `graphite_native_dawn` target and avoids
  linking Dawn Native for the Emscripten platform.
- A non-yielding Graphite context is intentional: `fTick = nullptr` and every
  submission uses `SyncToCpu::kNo`. Skia documents this as the supported way
  to run Graphite/Dawn on WebGPU without `ASYNCIFY`.

## `JsValStore` finding

`JsValStore` must not be assumed to exist on an arbitrary generated module.
CanvasKit makes it available by linking with:

```text
-s USE_WEBGPU=1
-s EXPORTED_RUNTIME_METHODS=WebGPU,JsValStore
```

CanvasKit's `webgpu.js` then calls `this.JsValStore.add(texture)` and native
code releases the handle with `emscripten_webgpu_release_js_handle`.

This project now uses the same explicit export in `BUILD.bazel`. C++ accesses
`emscripten::val::module_property("JsValStore")`, checks that the property is
neither null nor undefined, imports the texture, and immediately releases the
JS-store handle. No extra JS assignment is needed because
`EXPORTED_RUNTIME_METHODS` is the code that publishes the runtime helper on the
module object.

Official references:

- <https://skia.googlesource.com/skia/+/refs/heads/main/modules/canvaskit/webgpu.js>
- <https://skia.googlesource.com/skia/+/refs/heads/main/modules/canvaskit/BUILD.gn>
- <https://skia.googlesource.com/skia/+/refs/heads/main/include/gpu/graphite/dawn/DawnBackendContext.h>

## Files implementing the path

- `graphics/graphite_canvas_context.{h,cc}`: device/context setup, current
  texture import, SkSurface wrapping, and Graphite submission.
- `fiddles/skia_pulse_fiddle.{h,cc}`: animated SkCanvas drawing and accumulated
  timing output every ten frames.
- `canvas-worker/src/worker.ts`: creates/configures WebGPU on the
  OffscreenCanvas and passes `preinitializedWebGPUDevice` to Emscripten.
- `third_party/skia_graphite_dawn.patch`: publishes the missing Bazel target
  and its private dependencies.
- `SKIA_CANVAS_WIRING.md`: user-facing explanation and minimal C++ example.

## Last build result and next step

The command was:

```sh
cd cc-engine
bazel build -c opt //:demo_wasm --verbose_failures
```

It compiled 300+ Skia actions and failed while compiling
`DawnCaps.cpp` because `DawnGraphicsPipeline.h` includes
`include/ports/SkCFObject.h`, but the new target did not expose
`//include/ports:core_foundation_hdrs`. That filegroup has now been added to
the target's `hdrs` in the patch.

Resume with:

```sh
cd cc-engine
bazel build -c opt //:demo_wasm --verbose_failures
```

Bazel should reuse the completed actions. If another missing private
filegroup appears, add only that owning target to `graphite_native_dawn`;
avoid broad visibility changes.

After the build succeeds:

```sh
./scripts/build-and-export.sh
cd ..
bun run check
bun run build
```

Then run the app in a WebGPU-capable Chromium browser, select **Skia Pulse**,
and verify:

1. the worker logs WebGPU configuration;
2. C++ logs `Skia Graphite/Dawn WebGPU context ready`;
3. animated shapes are visible;
4. the ten-frame timing line repeats;
5. there are no validation errors in the browser console.

## Checks completed in this pass

`bun run check` passes for both the worker and Svelte app (zero Svelte errors
and warnings). The generated Wasm artifacts currently in
`canvas-worker/src/wasm` are still the previous build until the Graphite build
and exporter complete.
