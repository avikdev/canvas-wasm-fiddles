#include "graphics/graphite_canvas_context.h"

#include <iostream>
#include <memory>
#include <utility>

#include <emscripten/html5_webgpu.h>
#include <emscripten/val.h>
#include <webgpu/webgpu_cpp.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/graphite/BackendTexture.h"
#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/ContextOptions.h"
#include "include/gpu/graphite/GraphiteTypes.h"
#include "include/gpu/graphite/Recorder.h"
#include "include/gpu/graphite/Recording.h"
#include "include/gpu/graphite/Surface.h"
#include "include/gpu/graphite/dawn/DawnBackendContext.h"
#include "include/gpu/graphite/dawn/DawnGraphiteTypes.h"

GraphiteCanvasContext::GraphiteCanvasContext() = default;

GraphiteCanvasContext::~GraphiteCanvasContext() = default;

bool GraphiteCanvasContext::Initialize() {
  wgpu::Device device =
      wgpu::Device::Acquire(emscripten_webgpu_get_device());
  if (!device) {
    std::cerr << "[cc-engine/stderr] Emscripten did not provide the "
                 "preinitialized WebGPU device."
              << std::endl;
    return false;
  }

  skgpu::graphite::DawnBackendContext backend_context;
  backend_context.fDevice = device;
  backend_context.fQueue = device.GetQueue();
  backend_context.fTick = nullptr;

  skgpu::graphite::ContextOptions options;
  context_ =
      skgpu::graphite::ContextFactory::MakeDawn(backend_context, options);
  if (context_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not create a Graphite/Dawn "
                 "context."
              << std::endl;
    return false;
  }

  recorder_ = context_->makeRecorder();
  if (recorder_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not create a Graphite "
                 "recorder."
              << std::endl;
    context_.reset();
    return false;
  }

  std::cout << "[cc-engine/stdout] Skia Graphite/Dawn WebGPU context ready."
            << std::endl;
  return true;
}

sk_sp<SkSurface> GraphiteCanvasContext::AcquireSurface(
    const emscripten::val& canvas_context,
    std::string_view texture_format) {
  if (recorder_ == nullptr) {
    return nullptr;
  }

  emscripten::val texture =
      canvas_context.call<emscripten::val>("getCurrentTexture");
  if (texture.isNull() || texture.isUndefined()) {
    std::cerr << "[cc-engine/stderr] WebGPU returned no current canvas "
                 "texture."
              << std::endl;
    return nullptr;
  }

  emscripten::val value_store =
      emscripten::val::module_property("JsValStore");
  if (value_store.isNull() || value_store.isUndefined()) {
    std::cerr << "[cc-engine/stderr] Emscripten's JsValStore runtime method "
                 "was not exported on the module."
              << std::endl;
    return nullptr;
  }

  const int texture_handle = value_store.call<int>("add", texture);
  WGPUTexture imported_texture =
      emscripten_webgpu_import_texture(texture_handle);
  emscripten_webgpu_release_js_handle(texture_handle);

  if (imported_texture == nullptr) {
    std::cerr << "[cc-engine/stderr] Could not import the current WebGPU "
                 "canvas texture."
              << std::endl;
    return nullptr;
  }

  wgpu::Texture dawn_texture = wgpu::Texture::Acquire(imported_texture);
  skgpu::graphite::BackendTexture backend_texture =
      skgpu::graphite::BackendTextures::MakeDawn(dawn_texture.Get());
  if (!backend_texture.isValid()) {
    std::cerr << "[cc-engine/stderr] Skia rejected the WebGPU canvas "
                 "texture."
              << std::endl;
    return nullptr;
  }

  const SkColorType color_type =
      texture_format == "rgba8unorm" ? kRGBA_8888_SkColorType
                                     : kBGRA_8888_SkColorType;
  sk_sp<SkColorSpace> color_space = SkColorSpace::MakeSRGB();
  sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
      recorder_.get(), backend_texture, color_type, std::move(color_space),
      nullptr, nullptr, nullptr, "canvas-swapchain");
  if (surface == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not wrap the current WebGPU "
                 "canvas texture."
              << std::endl;
  }
  return surface;
}

bool GraphiteCanvasContext::Submit() {
  if (context_ == nullptr || recorder_ == nullptr) {
    return false;
  }

  std::unique_ptr<skgpu::graphite::Recording> recording =
      recorder_->snap();
  if (recording == nullptr) {
    std::cerr << "[cc-engine/stderr] Graphite produced no recording."
              << std::endl;
    return false;
  }

  skgpu::graphite::InsertRecordingInfo recording_info;
  recording_info.fRecording = recording.get();
  if (context_->insertRecording(recording_info) !=
      skgpu::graphite::InsertStatus::kSuccess) {
    std::cerr << "[cc-engine/stderr] Graphite rejected the frame recording."
              << std::endl;
    return false;
  }

  if (!context_->submit(skgpu::graphite::SyncToCpu::kNo)) {
    std::cerr << "[cc-engine/stderr] Graphite could not submit the frame."
              << std::endl;
    return false;
  }
  return true;
}
