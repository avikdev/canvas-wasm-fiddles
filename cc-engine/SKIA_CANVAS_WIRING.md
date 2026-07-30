# Skia Graphite/Dawn to an HTML canvas

The Skia fiddle uses Skia's native C++ Graphite renderer through Dawn/WebGPU.
It is compiled into the fiddle engine's WebAssembly module; CanvasKit is not
loaded and no CPU pixel buffer is copied into the canvas.

## Browser-to-Skia wiring

1. The Svelte app transfers its `HTMLCanvasElement` to an `OffscreenCanvas` and
   sends it to the worker.
2. For the Skia fiddle, the worker requests a `GPUAdapter` and `GPUDevice`, gets
   the canvas's `GPUCanvasContext`, and configures it with the browser's
   preferred texture format.
3. The device is passed as Emscripten's `preinitializedWebGPUDevice`. The
   Emscripten 4.0.17 built-in WebGPU support makes that JavaScript device
   available through the WebGPU C/C++ API.
4. C++ obtains the device with `emscripten_webgpu_get_device()` and constructs
   a `skgpu::graphite::DawnBackendContext`.
5. Skia creates one long-lived Graphite `Context` and `Recorder`.
6. Every animation frame, C++ calls `GPUCanvasContext.getCurrentTexture()`
   through `emscripten::val`, imports the returned JavaScript `GPUTexture`
   through Emscripten's WebGPU bridge, and wraps it as a Graphite
   `BackendTexture`.
7. `SkSurfaces::WrapBackendTexture` creates the frame's GPU-backed `SkSurface`.
   Normal `SkCanvas` and `SkPaint` calls then record the animated drawing.
8. C++ snaps the recorder, inserts the recording into the Graphite context, and
   submits it without a CPU synchronization. The browser presents that current
   canvas texture.

```text
OffscreenCanvas.getContext("webgpu")
                |
                v
       browser GPUCanvasContext
                |
      getCurrentTexture() each frame
                |
                v
 JavaScript GPUTexture -- Emscripten import --> wgpu::Texture
                |
                v
  Graphite BackendTexture -> SkSurface -> SkCanvas drawing
                |
                v
       Graphite recording + submit
                |
                v
        browser presents texture
```

There is no `readPixels`, `ImageData`, staging 2D canvas, or final `drawImage`
copy in this path.

## Minimal C++ shape

Initialization:

```cpp
wgpu::Device device =
    wgpu::Device::Acquire(emscripten_webgpu_get_device());

skgpu::graphite::DawnBackendContext backend;
backend.fDevice = device;
backend.fQueue = device.GetQueue();

auto context = skgpu::graphite::ContextFactory::MakeDawn(
    backend, skgpu::graphite::ContextOptions{});
auto recorder = context->makeRecorder();
```

Per frame:

```cpp
emscripten::val texture = canvas_context.call<emscripten::val>(
    "getCurrentTexture");
emscripten::val value_store =
    emscripten::val::module_property("JsValStore");
if (value_store.isUndefined()) {
    return nullptr;
}
int handle = value_store.call<int>("add", texture);
WGPUTexture imported = emscripten_webgpu_import_texture(handle);
emscripten_webgpu_release_js_handle(handle);

wgpu::Texture dawn_texture = wgpu::Texture::Acquire(imported);
auto backend_texture =
    skgpu::graphite::BackendTextures::MakeDawn(dawn_texture.Get());

auto surface = SkSurfaces::WrapBackendTexture(
    recorder.get(),
    backend_texture,
    kBGRA_8888_SkColorType,
    SkColorSpace::MakeSRGB(),
    nullptr,
    nullptr,
    nullptr,
    "canvas-swapchain");

SkCanvas* canvas = surface->getCanvas();
canvas->clear(SK_ColorBLACK);
SkPaint paint;
paint.setAntiAlias(true);
paint.setColor(SK_ColorCYAN);
canvas->drawCircle(100, 100, 48, paint);

auto recording = recorder->snap();
skgpu::graphite::InsertRecordingInfo info;
info.fRecording = recording.get();
context->insertRecording(info);
context->submit(skgpu::graphite::SyncToCpu::kNo);
```

The real implementation retains the Graphite context and recorder, checks every
interop result, and imports a fresh current canvas texture for every frame.

## Build integration

Skia's Bazel `graphite_native_dawn` library supplies the Graphite/Dawn backend.
For an Emscripten target, native Dawn is deliberately not linked. Instead, the
Emscripten 4.0.17 sysroot provides the browser-facing WebGPU headers and
JavaScript runtime. The link uses `-s USE_WEBGPU=1` and explicitly exports
`WebGPU,JsValStore`, matching CanvasKit's own WebGPU build. `JsValStore` is
therefore a deliberate module export rather than an assumed global. The local
Skia patch exposes the Graphite/Dawn target without adding a second vendored
WebGPU implementation.

The worker creates a fresh worker and `OffscreenCanvas` when changing between a
2D fiddle and the WebGPU fiddle. Browser canvases cannot safely change from a
2D context to a WebGPU context after a context has already been created.
