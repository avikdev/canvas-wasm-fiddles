#pragma once

#include <emscripten/html5_webgl.h>
#include <emscripten/val.h>

#include "include/core/SkRefCnt.h"

class GrDirectContext;
class SkSurface;

struct WebGlPresentResult {
  bool success = false;
  double flush_ms = 0.0;
  double submit_ms = 0.0;
};

// Owns the Emscripten WebGL context and a Ganesh surface that wraps the
// browser canvas's default framebuffer (FBO 0).
class WebGlCanvasContext final {
public:
  WebGlCanvasContext();
  ~WebGlCanvasContext();

  WebGlCanvasContext(const WebGlCanvasContext &) = delete;
  WebGlCanvasContext &operator=(const WebGlCanvasContext &) = delete;

  bool Initialize(const emscripten::val &canvas);
  SkSurface *AcquireSurface(int width, int height);
  WebGlPresentResult FlushAndPresent();

private:
  bool MakeCurrent();
  bool RecreateSurface(int width, int height);

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context_ = 0;
  sk_sp<GrDirectContext> direct_context_;
  sk_sp<SkSurface> surface_;
  int surface_width_ = 0;
  int surface_height_ = 0;
  int webgl_version_ = 0;
};
