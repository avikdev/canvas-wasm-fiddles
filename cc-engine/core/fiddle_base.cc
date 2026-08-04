#include "core/fiddle_base.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>

FiddleBase::FiddleBase(FiddleBackend backend) : backend_(backend) {}

FiddleBackend FiddleBase::Backend() const { return backend_; }

bool FiddleBase::PopulateCanvas(std::unique_ptr<FiddleCanvasResource> canvas) {
  if (canvas == nullptr || canvas->Backend() != backend_) {
    std::cerr << "[cc-engine/stderr] Fiddle received an incompatible canvas "
                 "resource."
              << std::endl;
    return false;
  }

  canvas_ = std::move(canvas);
  RefreshDimensions();
  return true;
}

void FiddleBase::Resize(double width, double height,
                        double device_pixel_ratio) {
  assert(canvas_ != nullptr);
  device_pixel_ratio_ = std::max(1.0, device_pixel_ratio);
  const int pixel_width =
      std::max(1, static_cast<int>(std::round(width * device_pixel_ratio_)));
  const int pixel_height =
      std::max(1, static_cast<int>(std::round(height * device_pixel_ratio_)));
  if (!canvas_->Resize(pixel_width, pixel_height)) {
    std::cerr << "[cc-engine/stderr] Could not resize the canvas resource to "
              << pixel_width << "x" << pixel_height << "." << std::endl;
  }
  RefreshDimensions();
}

int FiddleBase::PixelWidth() const {
  assert(canvas_ != nullptr);
  return canvas_->PixelWidth();
}

int FiddleBase::PixelHeight() const {
  assert(canvas_ != nullptr);
  return canvas_->PixelHeight();
}

double FiddleBase::Width() const { return width_; }

double FiddleBase::Height() const { return height_; }

FiddleCanvasResource &FiddleBase::CanvasResource() {
  assert(canvas_ != nullptr);
  return *canvas_;
}

void FiddleBase::RefreshDimensions() {
  if (canvas_ == nullptr) {
    return;
  }
  width_ = static_cast<double>(canvas_->PixelWidth()) / device_pixel_ratio_;
  height_ = static_cast<double>(canvas_->PixelHeight()) / device_pixel_ratio_;
}

FiddleBaseWebGL::FiddleBaseWebGL() : FiddleBase(FiddleBackend::kWebGl) {}

WebGlCanvasResource &FiddleBaseWebGL::WebGlResource() {
  return static_cast<WebGlCanvasResource &>(CanvasResource());
}

FiddleBaseCpu::FiddleBaseCpu() : FiddleBase(FiddleBackend::kCpu) {}

CpuCanvasResource &FiddleBaseCpu::CpuResource() {
  return static_cast<CpuCanvasResource &>(CanvasResource());
}
