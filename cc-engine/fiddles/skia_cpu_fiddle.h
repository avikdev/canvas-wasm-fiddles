#pragma once

#include "core/fiddle_base.h"
#include "fiddles/skia_pulse_benchmark.h"
#include "graphics/raster_canvas_context.h"

class SkiaCpuFiddle final : public FiddleBaseCpu {
public:
  bool IsSvgWritable() const override { return true; }
  void Render(double time_seconds) override;

private:
  bool UpdateState(double time_seconds, int width, int height);
  void DrawFrame(SkCanvas *canvas, int width, int height) override;

  RasterCanvasContext raster_;
  SkiaPulseBenchmark benchmark_{"CPU / Raster + 2D upload"};
  double time_seconds_ = 0.0;
};
