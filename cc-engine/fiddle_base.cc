#include "fiddle_base.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

void FiddleBase::PopulateCanvas(const emscripten::val &canvas) {
  canvas_ = canvas;
  if (UsesWebGl()) {
    // Emscripten must create and register the WebGL context so native GL calls
    // and Skia share the same state. WebGlCanvasContext does that lazily.
    context_ = emscripten::val::undefined();
    RefreshDimensions();
    return;
  }

  const std::string context_type = "2d";
  context_ = canvas_.call<emscripten::val>("getContext", context_type);
  if (context_.isNull() || context_.isUndefined()) {
    std::cerr << "[cc-engine/stderr] Unable to create OffscreenCanvas "
              << context_type << " context." << std::endl;
    std::abort();
  }
  RefreshDimensions();
}

void FiddleBase::Resize(double width, double height,
                        double device_pixel_ratio) {
  device_pixel_ratio_ = std::max(1.0, device_pixel_ratio);
  const int pixel_width =
      std::max(1, static_cast<int>(std::round(width * device_pixel_ratio_)));
  const int pixel_height =
      std::max(1, static_cast<int>(std::round(height * device_pixel_ratio_)));
  if (canvas_["width"].as<int>() != pixel_width) {
    canvas_.set("width", pixel_width);
  }
  if (canvas_["height"].as<int>() != pixel_height) {
    canvas_.set("height", pixel_height);
  }
  if (!UsesWebGl()) {
    context_.call<void>("setTransform", device_pixel_ratio_, 0.0, 0.0,
                        device_pixel_ratio_, 0.0, 0.0);
  }
  RefreshDimensions();
}

bool FiddleBase::UsesWebGl() const { return false; }

emscripten::val &FiddleBase::Canvas() { return canvas_; }

emscripten::val &FiddleBase::Context() { return context_; }

double FiddleBase::Width() const { return width_; }

double FiddleBase::Height() const { return height_; }

void FiddleBase::RefreshDimensions() {
  width_ = canvas_["width"].as<double>() / device_pixel_ratio_;
  height_ = canvas_["height"].as<double>() / device_pixel_ratio_;
}
