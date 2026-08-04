#include "graphics/raster_canvas_context.h"

#include <algorithm>
#include <iostream>

#include "core/fiddle_base.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

RasterCanvasContext::RasterCanvasContext() = default;

RasterCanvasContext::~RasterCanvasContext() = default;

SkSurface *RasterCanvasContext::AcquireSurface(int width, int height) {
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

void RasterCanvasContext::Present(CpuCanvasResource &resource) {
  resource.PresentPixels(pixels_.data(), surface_width_, surface_height_,
                         pixels_.size());
}

bool RasterCanvasContext::RecreateSurface(int width, int height) {
  surface_.reset();
  const SkImageInfo image_info =
      SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());
  row_bytes_ = image_info.minRowBytes();
  pixels_.assign(image_info.computeByteSize(row_bytes_), 0);
  surface_ =
      SkSurfaces::WrapPixels(image_info, pixels_.data(), row_bytes_, nullptr);
  if (surface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not wrap the CPU raster pixel "
                 "buffer as an SkSurface."
              << std::endl;
    return false;
  }

  surface_width_ = width;
  surface_height_ = height;
  std::cout << "[cc-engine/stdout] CPU raster surface: Wasm RGBA buffer, size="
            << width << "x" << height
            << "; presentation uses ImageData/putImageData." << std::endl;
  return true;
}
