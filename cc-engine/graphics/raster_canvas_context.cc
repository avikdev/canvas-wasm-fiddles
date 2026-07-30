#include "graphics/raster_canvas_context.h"

#include <algorithm>
#include <iostream>

#include <emscripten/emscripten.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

namespace {

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

} // namespace

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

void RasterCanvasContext::Present(const emscripten::val &context) {
  PutRasterPixels(context.as_handle(),
                  reinterpret_cast<std::uintptr_t>(pixels_.data()),
                  surface_width_, surface_height_, pixels_.size());
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
