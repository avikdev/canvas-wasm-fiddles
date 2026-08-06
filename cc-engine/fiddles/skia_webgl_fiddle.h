#pragma once

#include <memory>

#include "core/fiddle_base.h"
#include "fiddles/skia_pulse_benchmark.h"

class WebGlCanvasContext;

class SkiaWebGlFiddle final : public FiddleBaseWebGL {
public:
  SkiaWebGlFiddle();
  ~SkiaWebGlFiddle() override;

  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool EnsureWebGl();
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  std::unique_ptr<WebGlCanvasContext> webgl_;
  bool initialization_attempted_ = false;
  double time_seconds_ = 0.0;
  SkiaPulseBenchmark benchmark_{"GPU / Ganesh WebGL"};
};
