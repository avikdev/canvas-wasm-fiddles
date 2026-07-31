# Skia drawing: WebGL versus CPU

The app contains two benchmark entries:

- **Skia Drawing (WebGL)** (`skia-webgl`) uses Ganesh and renders directly into
  WebGL default framebuffer 0.
- **Skia Drawing (CPU)** (`skia-cpu`) uses a raster `SkSurface` backed by an RGBA
  buffer in Wasm memory, then presents it with `putImageData`.

Both call `DrawSkiaDrawing()` in `fiddles/skia_drawing_builder.cc`. There are no
backend-specific branches in the scene, so the clear, 14 halos, 24 curved
rays, 24 nodes, and central rounded rectangle are identical.

## Aligned timing windows

Each fiddle accumulates 120 frames and prints the same primary fields:

```text
Skia drawing benchmark [backend] last 120 frames (accumulated ms):
surface=..., scene-draw=..., present=..., measured-total=..., avg/frame=...
```

The measurements use `std::chrono::steady_clock` around the same boundaries:

| Stage | Ganesh/WebGL | CPU raster |
| --- | --- | --- |
| `surface` | Make the WebGL context current and acquire/recreate the FBO 0 wrapper | Acquire/recreate the Wasm-backed raster surface |
| `scene-draw` | Call the shared scene; Ganesh mostly records deferred draw operations | Call the shared scene; Skia rasterizes into Wasm memory immediately |
| `present` | Make current, flush recorded work, and submit without a CPU wait | Expose the existing Wasm buffer as `ImageData` and call `putImageData` |

`measured-total` is the sum of those three non-overlapping stages. The WebGL
log also includes `present-breakdown(flush=..., submit=...)`; this breakdown is
diagnostic and is already contained in `present`, so it is not added to the
total a second time.

This is an aligned application-pipeline comparison, not a claim that the
stages do equivalent work. In particular, `scene-draw` is deferred on Ganesh
but eager on raster. WebGL `submit(kNo)` also does not wait for the GPU or
browser compositor to finish, while CPU `putImageData` measures the browser
API call. GPU execution time would require asynchronous timer queries and is a
different metric.

## Observed browser run

The rebuilt app was run at a 1664x909 canvas with Emscripten assertions
enabled. These are the later, warmed 120-frame windows from the same browser
session:

| Backend | `surface` | `scene-draw` | `present` | Total | Average/frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| GPU / Ganesh WebGL | 1.10 ms | 90.10 ms | 94.70 ms | 185.90 ms | 1.55 ms |
| CPU / Raster + 2D upload | 0.80 ms | 1606.70 ms | 285.70 ms | 1893.20 ms | 15.78 ms |

The warmed WebGL application-pipeline measurement was about 10.2x faster. Its
`present` breakdown was `flush=93.60 ms, submit=0.10 ms`; the remaining 1.00 ms
in the outer `present` timer includes making the context current and timing/call
overhead.

Warm-up mattered substantially. The first WebGL window reported 9.48 ms/frame,
then later windows reported 1.74 and 1.55 ms/frame. Compare warmed windows and
do not interpret one run as a portable GPU/CPU score; viewport size, browser,
hardware, power state, and build flags all affect it.

## What the old `flush-submit` number contained

The earlier WebGL timer surrounded all of `FlushAndSubmit()` plus context
selection and a `glGetError()` call. In the supplied sample:

```text
surface=0.80, record-draw=179.30, flush-submit=4692.50,
measured-total=4872.60, avg/frame=40.60
```

`flush-submit` was 39.10 ms/frame and about 96.3% of the measured total. That
does not mean a 39.10 ms texture copy occurred. Ganesh is a deferred renderer:
most of the work implied by the `SkCanvas` calls is deliberately moved from
`scene-draw` into `flush`.

For the pinned Skia revision, the relevant call path is:

```text
GrDirectContext::flush(surface)
  -> flush the surface's drawing manager/render tasks
  -> prepare paths, meshes, pipelines, and batches
  -> issue WebGL state changes, buffer operations, and draw calls

GrDirectContext::submit(GrSyncCpu::kNo)
  -> GL backend submit
  -> glFlush()
  -> no glFinish() and no intentional CPU wait
```

The updated instrumentation times `flush` and `submit` separately. It also
removes the application's per-frame `glGetError()` from the presentation
timer. The build still uses Emscripten assertions, and Skia performs its own
backend error handling.

## Copy audit

The WebGL application path is already the direct path available through this
API:

- The `SkSurface` wraps FBO 0 with
  `SkSurfaces::WrapBackendRenderTarget`; it is not a Skia-owned presentation
  texture.
- The scene contains no images and calls no `readPixels`, `writePixels`,
  `ImageData`, `drawImage`, or texture-upload API.
- The context requests `antialias: false`; the observed sample count is logged
  when the surface is created, so an application-requested multisample resolve
  is absent when it reports zero.
- `renderViaOffscreenBackBuffer` and `explicitSwapControl` are disabled.
- `submit(kNo)` uses `glFlush`, not `glFinish`; there is no deliberate
  readback or GPU-to-CPU synchronization.

Therefore, no application-level full-frame copy occurs during Ganesh flush or
submit. Buffer uploads for generated vertices or uniforms can occur, but they
are command data, not a copy of the rendered canvas texture. WebGL does not
expose the browser's swapchain image, so an internal browser/compositor/driver
copy may still occur and cannot be proved absent or controlled from Skia.

The CPU variant has one unavoidable full-frame presentation copy:

1. Skia draws unpremultiplied RGBA directly into the vector's Wasm memory;
   there is no Skia staging surface and the alpha representation matches
   `ImageData`.
2. `Uint8ClampedArray` and `ImageData` reuse that same Wasm allocation without
   cloning it.
3. `putImageData` copies the RGBA frame into the browser canvas backing store.

## Can the expensive WebGL stage be removed?

Not while retaining deferred Ganesh rendering and displaying every animation
frame. A flush is what turns the recorded Skia operations into WebGL commands,
and a submit makes those commands available to the browser's GL
implementation.

The current path already avoids the removable full-frame copies. Further
performance work should target the work inside flush rather than presentation
copy removal:

- compare warmed 120-frame windows and disregard shader/pipeline warm-up;
- reduce dynamic path generation or cache reusable geometry;
- reduce draw count and state changes so Ganesh can batch more effectively;
- build a non-assertion benchmark variant to quantify validation overhead;
- add `EXT_disjoint_timer_query_webgl2` only if actual GPU execution time,
  rather than CPU-side submission cost, is the question.
