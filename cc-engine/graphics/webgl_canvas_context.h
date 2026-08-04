#pragma once

#include "include/core/SkRefCnt.h"

class GrDirectContext;
class SkSurface;
class WebGlCanvasResource;

struct WebGlPresentResult {
  bool success = false;
  double flush_ms = 0.0;
  double submit_ms = 0.0;
};

// Owns a Ganesh surface that wraps a platform-provided WebGL default
// framebuffer (FBO 0). The platform context itself remains in the resource.
class WebGlCanvasContext final {
public:
  WebGlCanvasContext();
  ~WebGlCanvasContext();

  WebGlCanvasContext(const WebGlCanvasContext &) = delete;
  WebGlCanvasContext &operator=(const WebGlCanvasContext &) = delete;

  bool Initialize(WebGlCanvasResource &resource);
  SkSurface *AcquireSurface(int width, int height);
  WebGlPresentResult FlushAndPresent();

private:
  bool MakeCurrent();
  bool RecreateSurface(int width, int height);

  WebGlCanvasResource *resource_ = nullptr;
  sk_sp<GrDirectContext> direct_context_;
  sk_sp<SkSurface> surface_;
  int surface_width_ = 0;
  int surface_height_ = 0;
};
