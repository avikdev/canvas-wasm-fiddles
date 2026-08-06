#pragma once

#include <memory>
#include <string>

#include "core/fiddle_base.h"
#include "core/fiddle_registry.h"

class FiddleManager {
public:
  FiddleManager(FiddleCanvasResourceProvider &canvas_resources,
                double initial_width, double initial_height,
                const std::string &initial_key);

  bool SelectFiddle(const std::string &key);
  bool IsSvgWritable() const;
  std::string ExportSvg();
  void Resize(double width, double height, double device_pixel_ratio);
  void Tick(double delta_seconds);

private:
  FiddleCanvasResourceProvider &canvas_resources_;
  FiddleRegistry registry_;
  std::unique_ptr<FiddleBase> active_fiddle_;
  std::string active_key_;
  double width_ = 1.0;
  double height_ = 1.0;
  double device_pixel_ratio_ = 1.0;
  double time_seconds_ = 0.0;
};
