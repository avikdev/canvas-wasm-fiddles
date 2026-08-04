#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "include/core/SkRefCnt.h"

class CpuCanvasResource;
class SkSurface;

// A CPU SkSurface whose pixels live directly in Wasm linear memory. Presenting
// exposes that memory to ImageData without a staging copy; putImageData then
// performs the required Wasm-to-browser-canvas copy.
class RasterCanvasContext final {
public:
  RasterCanvasContext();
  ~RasterCanvasContext();

  SkSurface *AcquireSurface(int width, int height);
  void Present(CpuCanvasResource &resource);

private:
  bool RecreateSurface(int width, int height);

  std::vector<std::uint8_t> pixels_;
  sk_sp<SkSurface> surface_;
  std::size_t row_bytes_ = 0;
  int surface_width_ = 0;
  int surface_height_ = 0;
};
