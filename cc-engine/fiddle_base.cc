#include "fiddle_base.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

void FiddleBase::PopulateCanvas(const emscripten::val& canvas) {
  canvas_ = canvas;
  context_ =
      canvas_.call<emscripten::val>("getContext", std::string("2d"));
  if (context_.isNull() || context_.isUndefined()) {
    std::cerr << "[cc-engine/stderr] Unable to create OffscreenCanvas 2D "
                 "context."
              << std::endl;
    std::abort();
  }
  RefreshDimensions();
}

void FiddleBase::Resize(double width, double height,
                        double device_pixel_ratio) {
  device_pixel_ratio_ = std::max(1.0, device_pixel_ratio);
  canvas_.set("width",
              std::max(1, static_cast<int>(std::round(
                              width * device_pixel_ratio_))));
  canvas_.set("height",
              std::max(1, static_cast<int>(std::round(
                              height * device_pixel_ratio_))));
  context_.call<void>("setTransform", device_pixel_ratio_, 0.0, 0.0,
                      device_pixel_ratio_, 0.0, 0.0);
  RefreshDimensions();
}

emscripten::val& FiddleBase::Canvas() { return canvas_; }

emscripten::val& FiddleBase::Context() { return context_; }

double FiddleBase::Width() const { return width_; }

double FiddleBase::Height() const { return height_; }

void FiddleBase::RefreshDimensions() {
  width_ = canvas_["width"].as<double>() / device_pixel_ratio_;
  height_ = canvas_["height"].as<double>() / device_pixel_ratio_;
}
