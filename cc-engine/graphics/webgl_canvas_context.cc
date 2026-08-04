#include "graphics/webgl_canvas_context.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

#include <GLES2/gl2.h>

#include "core/fiddle_base.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#include "src/gpu/ganesh/gl/GrGLDefines.h"

namespace {

using TimingClock = std::chrono::steady_clock;

double ElapsedMilliseconds(TimingClock::time_point start,
                           TimingClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

WebGlCanvasContext::WebGlCanvasContext() = default;

WebGlCanvasContext::~WebGlCanvasContext() {
  if (resource_ == nullptr) {
    return;
  }

  if (!MakeCurrent() && direct_context_ != nullptr) {
    direct_context_->abandonContext();
  }
  surface_.reset();
  direct_context_.reset();
}

bool WebGlCanvasContext::Initialize(WebGlCanvasResource &resource) {
  if (direct_context_ != nullptr) {
    return true;
  }
  resource_ = &resource;
  if (!MakeCurrent()) {
    return false;
  }

  sk_sp<const GrGLInterface> interface = GrGLInterfaces::MakeWebGL();
  if (interface == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not create its WebGL "
                 "interface."
              << std::endl;
    return false;
  }

  direct_context_ = GrDirectContexts::MakeGL(std::move(interface));
  if (direct_context_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not create a Ganesh "
                 "GrDirectContext."
              << std::endl;
    return false;
  }

  std::cout << "[cc-engine/stdout] Skia Ganesh/WebGL " << resource_->Version()
            << " context ready." << std::endl;
  return true;
}

SkSurface *WebGlCanvasContext::AcquireSurface(int width, int height) {
  if (direct_context_ == nullptr) {
    return nullptr;
  }
  if (!MakeCurrent()) {
    return nullptr;
  }

  width = std::max(1, width);
  height = std::max(1, height);
  if (surface_ == nullptr || width != surface_width_ ||
      height != surface_height_) {
    if (!RecreateSurface(width, height)) {
      return nullptr;
    }
  }
  return surface_.get();
}

WebGlPresentResult WebGlCanvasContext::FlushAndPresent() {
  if (surface_ == nullptr || !MakeCurrent()) {
    return {};
  }

  // Ganesh records draw operations until flush(). On GL, flush() executes that
  // work as WebGL calls; submit(kNo) then calls glFlush without waiting for GPU
  // completion. explicitSwapControl is disabled, so the browser presents FBO 0
  // at the end of this worker animation frame.
  const auto flush_start = TimingClock::now();
  direct_context_->flush(surface_.get());
  const auto submit_start = TimingClock::now();
  const bool submitted = direct_context_->submit(GrSyncCpu::kNo);
  const auto submit_end = TimingClock::now();

  return {.success = submitted,
          .flush_ms = ElapsedMilliseconds(flush_start, submit_start),
          .submit_ms = ElapsedMilliseconds(submit_start, submit_end)};
}

bool WebGlCanvasContext::MakeCurrent() {
  if (resource_ == nullptr) {
    return false;
  }
  if (!resource_->MakeCurrent()) {
    std::cerr << "[cc-engine/stderr] Could not make the platform WebGL "
                 "context current."
              << std::endl;
    return false;
  }
  return true;
}

bool WebGlCanvasContext::RecreateSurface(int width, int height) {
  surface_.reset();

  // This is CanvasKit's on-screen path: wrap WebGL's default framebuffer
  // directly instead of allocating a Skia-owned texture or CPU pixel buffer.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  direct_context_->resetContext(kRenderTarget_GrGLBackendState |
                                kMisc_GrGLBackendState);

  GLint sample_count = 0;
  GLint stencil_bits = 0;
  glGetIntegerv(GL_SAMPLES, &sample_count);
  glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = 0;
  framebuffer_info.fFormat = GR_GL_RGBA8;
  GrBackendRenderTarget target = GrBackendRenderTargets::MakeGL(
      width, height, sample_count, stencil_bits, framebuffer_info);
  if (!target.isValid()) {
    std::cerr << "[cc-engine/stderr] Skia rejected WebGL default framebuffer "
                 "0 as a backend render target."
              << std::endl;
    return false;
  }

  surface_ = SkSurfaces::WrapBackendRenderTarget(
      direct_context_.get(), target, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
  if (surface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not wrap WebGL default "
                 "framebuffer 0 as an SkSurface."
              << std::endl;
    return false;
  }

  surface_width_ = width;
  surface_height_ = height;
  std::cout << "[cc-engine/stdout] Direct GPU canvas surface: FBO=0, size="
            << width << "x" << height << ", samples=" << sample_count
            << ", stencil=" << stencil_bits
            << "; no CPU pixel buffer or intermediate Skia texture."
            << std::endl;
  return true;
}
