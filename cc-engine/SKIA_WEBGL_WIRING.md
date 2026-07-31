# Skia Ganesh/WebGL to an OffscreenCanvas

The `skia-webgl` fiddle runs native Skia C++ in Wasm and renders through
Ganesh's OpenGL backend into a worker-owned `OffscreenCanvas`. It follows the
same surface construction used by CanvasKit's `MakeWebGLCanvasSurface`.

## End-to-end path

1. The Svelte app transfers its `HTMLCanvasElement` to an `OffscreenCanvas`.
2. Embind passes that exact `OffscreenCanvas` object to C++.
3. C++ passes the object back through a tiny Emscripten interop helper that
   calls `GL.createContext(canvas, attributes)`. This both creates WebGL 2 and
   registers it with Emscripten's native GL dispatch, with a WebGL 1 fallback.
4. Skia's `GrGLInterfaces::MakeWebGL()` and
   `GrDirectContexts::MakeGL()` create the Ganesh GPU context.
5. C++ describes the canvas default framebuffer (`FBO 0`) as a
   `GrBackendRenderTarget`.
6. `SkSurfaces::WrapBackendRenderTarget()` encapsulates that on-screen target
   as a normal GPU-backed `SkSurface`.
7. The fiddle uses ordinary `SkCanvas`, `SkPaint`, and `SkPath` calls.
8. `GrDirectContext::flush(surface)` executes the recorded Ganesh work as
   WebGL calls, and `GrDirectContext::submit(kNo)` sends the command stream
   without waiting for GPU completion. The browser presents FBO 0 at the end
   of the worker animation frame.

```text
HTMLCanvasElement
      |
      | transferControlToOffscreen()
      v
worker OffscreenCanvas
      |
      | Emscripten GL.createContext(canvas, attributes)
      v
Emscripten-registered WebGL context
      |
      | GrGLInterfaces::MakeWebGL()
      v
Skia Ganesh GrDirectContext
      |
      | wrap default framebuffer 0
      v
GrBackendRenderTarget -> SkSurface -> SkCanvas
      |
      | flush(surface), then submit(kNo)
      v
browser presents the canvas framebuffer
```

## Direct-write and zero-copy boundary

The application does not allocate a CPU pixel buffer and does not use
`readPixels`, `writePixels`, `ImageData`, a 2D staging canvas, or `drawImage`.
It also does not allocate a Skia-owned intermediate texture for presentation.
Ganesh records GPU commands against the render target that represents WebGL's
on-screen default framebuffer.

The context requests `antialias = false`, so the application does not ask the
browser for an MSAA framebuffer that would require a resolve. It also requests
`preserveDrawingBuffer = false`, which allows normal swap behavior.

WebGL deliberately does not expose the browser's swapchain texture object.
Therefore, the strongest accurate guarantee at this layer is: **direct GPU
rendering to default framebuffer 0 with zero CPU-side pixel copies and no
application-level intermediate presentation texture**. A browser, compositor,
driver, or operating system can still perform internal copies that WebGL does
not expose or let the application control.

The native code logs the framebuffer ID, dimensions, sample count, and stencil
bits whenever the on-screen surface is created or recreated after resize.

## Minimal C++ shape

```cpp
auto webgl = CreateEmscriptenWebGlContext(canvas.as_handle(), 2);
emscripten_webgl_make_context_current(webgl);

auto interface = GrGLInterfaces::MakeWebGL();
auto context = GrDirectContexts::MakeGL(std::move(interface));

GrGLFramebufferInfo framebuffer;
framebuffer.fFBOID = 0;
framebuffer.fFormat = GR_GL_RGBA8;
auto target = GrBackendRenderTargets::MakeGL(
    width, height, sampleCount, stencilBits, framebuffer);

auto surface = SkSurfaces::WrapBackendRenderTarget(
    context.get(),
    target,
    kBottomLeft_GrSurfaceOrigin,
    kRGBA_8888_SkColorType,
    SkColorSpace::MakeSRGB(),
    nullptr);

surface->getCanvas()->drawCircle(100, 100, 48, paint);
context->flush(surface.get());
context->submit(GrSyncCpu::kNo);
```

The real implementation checks every interop result, keeps the WebGL and
Ganesh contexts alive, and recreates only the framebuffer wrapper when the
canvas pixel dimensions change.

## Build integration

The active Skia dependencies are:

```text
@skia//:core
@skia//:ganesh_gl
@skia//:ganesh_webgl_factory
```

Emscripten links with `-s MAX_WEBGL_VERSION=2` and `-s USE_WEBGL2=1`.
`MAX_WEBGL_VERSION=2` enables WebGL 2 APIs while retaining WebGL 1 support;
the second flag matches the pinned CanvasKit build and Emscripten toolchain.

Build and copy the generated loader and Wasm binary into the worker:

```sh
cd cc-engine
./scripts/build-and-export.sh
```

Then run the app and select **Skia Drawing (WebGL)**:

```sh
cd ..
bun run dev
```
