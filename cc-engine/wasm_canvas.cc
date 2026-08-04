#include "wasm_canvas.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include <emscripten/emscripten.h>
#include <emscripten/html5_webgl.h>

#include "core/fiddle_base.h"
#include "core/fiddle_manager.h"

namespace {

// Emscripten's selector-based context API cannot find an OffscreenCanvas in a
// worker. Pass the actual JavaScript canvas through Emval instead.
EM_JS(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE, CreateWebGlContext,
      (emscripten::EM_VAL canvas_handle, int major_version), {
        const canvas = Emval.toValue(canvas_handle);
        return GL.createContext(canvas, {
          alpha : true,
          depth : false,
          stencil : true,
          antialias : false,
          premultipliedAlpha : true,
          preserveDrawingBuffer : false,
          powerPreference : "default",
          failIfMajorPerformanceCaveat : false,
          majorVersion : major_version,
          minorVersion : 0,
          enableExtensionsByDefault : true,
          explicitSwapControl : false,
          proxyContextToMainThread : 0,
          renderViaOffscreenBackBuffer : false,
        });
      });

// clang-format off
EM_JS(void, PutRasterPixels,
      (emscripten::EM_VAL context_handle, std::uintptr_t pixel_pointer,
       int width, int height, std::size_t byte_length), {
        const context = Emval.toValue(context_handle);
        let frame = context.__skiaRasterFrame;
        if (!frame || frame.buffer !== HEAPU8.buffer ||
            frame.pixelPointer !== pixel_pointer || frame.width !== width ||
            frame.height !== height) {
          const pixels = new Uint8ClampedArray(
              HEAPU8.buffer, pixel_pointer, byte_length);
          frame = {
            buffer : HEAPU8.buffer,
            pixelPointer : pixel_pointer,
            width,
            height,
            imageData : new ImageData(pixels, width, height),
          };
          context.__skiaRasterFrame = frame;
        }
        context.putImageData(frame.imageData, 0, 0);
      });
// clang-format on

class WasmCanvasState {
public:
  explicit WasmCanvasState(emscripten::val canvas)
      : canvas_(std::move(canvas)), pixel_width_(canvas_["width"].as<int>()),
        pixel_height_(canvas_["height"].as<int>()) {}

  bool Resize(int pixel_width, int pixel_height) {
    pixel_width = std::max(1, pixel_width);
    pixel_height = std::max(1, pixel_height);
    if (pixel_width_ != pixel_width) {
      canvas_.set("width", pixel_width);
      pixel_width_ = pixel_width;
    }
    if (pixel_height_ != pixel_height) {
      canvas_.set("height", pixel_height);
      pixel_height_ = pixel_height;
    }
    return true;
  }

  int PixelWidth() const { return pixel_width_; }
  int PixelHeight() const { return pixel_height_; }
  emscripten::val &Canvas() { return canvas_; }

private:
  emscripten::val canvas_;
  int pixel_width_;
  int pixel_height_;
};

class WasmWebGlCanvasResource final : public WebGlCanvasResource {
public:
  explicit WasmWebGlCanvasResource(emscripten::val canvas)
      : state_(std::move(canvas)) {}

  ~WasmWebGlCanvasResource() override {
    if (webgl_context_ > 0) {
      emscripten_webgl_destroy_context(webgl_context_);
    }
  }

  bool Initialize() {
    webgl_context_ = CreateWebGlContext(state_.Canvas().as_handle(), 2);
    version_ = 2;
    if (webgl_context_ <= 0) {
      webgl_context_ = CreateWebGlContext(state_.Canvas().as_handle(), 1);
      version_ = 1;
    }
    if (webgl_context_ <= 0) {
      std::cerr << "[cc-engine/stderr] Emscripten could not create a WebGL "
                   "context for the worker OffscreenCanvas."
                << std::endl;
      return false;
    }
    return true;
  }

  bool Resize(int pixel_width, int pixel_height) override {
    return state_.Resize(pixel_width, pixel_height);
  }

  int PixelWidth() const override { return state_.PixelWidth(); }
  int PixelHeight() const override { return state_.PixelHeight(); }

  bool MakeCurrent() override {
    if (webgl_context_ <= 0) {
      return false;
    }
    const EMSCRIPTEN_RESULT result =
        emscripten_webgl_make_context_current(webgl_context_);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
      std::cerr << "[cc-engine/stderr] Emscripten could not make the WebGL "
                   "context current (result "
                << result << ")." << std::endl;
      return false;
    }
    return true;
  }

  int Version() const override { return version_; }

private:
  WasmCanvasState state_;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context_ = 0;
  int version_ = 0;
};

class WasmCpuCanvasResource final : public CpuCanvasResource {
public:
  explicit WasmCpuCanvasResource(emscripten::val canvas)
      : state_(std::move(canvas)),
        context_(state_.Canvas().call<emscripten::val>("getContext",
                                                       std::string("2d"))) {}

  bool IsValid() const { return !context_.isNull() && !context_.isUndefined(); }

  bool Resize(int pixel_width, int pixel_height) override {
    return state_.Resize(pixel_width, pixel_height);
  }

  int PixelWidth() const override { return state_.PixelWidth(); }
  int PixelHeight() const override { return state_.PixelHeight(); }

  void PresentPixels(const std::uint8_t *pixels, int width, int height,
                     std::size_t byte_length) override {
    PutRasterPixels(context_.as_handle(),
                    reinterpret_cast<std::uintptr_t>(pixels), width, height,
                    byte_length);
  }

private:
  WasmCanvasState state_;
  emscripten::val context_;
};

} // namespace

class WasmFiddleManager::ResourceProvider final
    : public FiddleCanvasResourceProvider {
public:
  explicit ResourceProvider(emscripten::val canvas)
      : canvas_(std::move(canvas)) {}

  std::unique_ptr<FiddleCanvasResource> Create(FiddleBackend backend) override {
    if (backend == FiddleBackend::kWebGl) {
      auto resource = std::make_unique<WasmWebGlCanvasResource>(canvas_);
      if (!resource->Initialize()) {
        return nullptr;
      }
      return resource;
    }

    auto resource = std::make_unique<WasmCpuCanvasResource>(canvas_);
    if (!resource->IsValid()) {
      std::cerr << "[cc-engine/stderr] Unable to create an OffscreenCanvas 2D "
                   "context."
                << std::endl;
      return nullptr;
    }
    return resource;
  }

private:
  emscripten::val canvas_;
};

WasmFiddleManager::WasmFiddleManager(emscripten::val canvas,
                                     const std::string &initial_key) {
  const double initial_width = canvas["width"].as<double>();
  const double initial_height = canvas["height"].as<double>();
  canvas_resources_ = std::make_unique<ResourceProvider>(std::move(canvas));
  manager_ = std::make_unique<FiddleManager>(*canvas_resources_, initial_width,
                                             initial_height, initial_key);
}

WasmFiddleManager::~WasmFiddleManager() = default;

bool WasmFiddleManager::SelectFiddle(const std::string &key) {
  return manager_->SelectFiddle(key);
}

void WasmFiddleManager::Resize(double width, double height,
                               double device_pixel_ratio) {
  manager_->Resize(width, height, device_pixel_ratio);
}

void WasmFiddleManager::Tick(double delta_seconds) {
  manager_->Tick(delta_seconds);
}
