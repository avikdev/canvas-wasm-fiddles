#pragma once

#include <memory>

#include "core/fiddle_base.h"
#include "fiddles/skia_pulse_benchmark.h"

class WebGlCanvasContext;

class SkiaWebGlFiddle final : public FiddleBaseWebGL {
public:
  SkiaWebGlFiddle();
  ~SkiaWebGlFiddle() override;

  void Render(double time_seconds) override;

private:
  bool EnsureWebGl();

  std::unique_ptr<WebGlCanvasContext> webgl_;
  bool initialization_attempted_ = false;
  SkiaPulseBenchmark benchmark_{"GPU / Ganesh WebGL"};
};
