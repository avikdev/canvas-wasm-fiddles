#pragma once

#include <memory>
#include <string>

#include <emscripten/val.h>

#include "fiddle_base.h"
#include "fiddle_registry.h"

class FiddleManager {
 public:
  explicit FiddleManager(emscripten::val canvas);

  bool SelectFiddle(const std::string& key);
  void Resize(double width, double height, double device_pixel_ratio);
  void Tick(double delta_seconds);

 private:
  emscripten::val canvas_;
  FiddleRegistry registry_;
  std::unique_ptr<FiddleBase> active_fiddle_;
  std::string active_key_;
  double width_ = 1.0;
  double height_ = 1.0;
  double device_pixel_ratio_ = 1.0;
  double time_seconds_ = 0.0;
};
