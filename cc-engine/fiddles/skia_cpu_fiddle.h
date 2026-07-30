#pragma once

#include "fiddle_base.h"
#include "fiddles/skia_pulse_benchmark.h"
#include "graphics/raster_canvas_context.h"

class SkiaCpuFiddle final : public FiddleBase {
public:
  void Render(double time_seconds) override;

private:
  RasterCanvasContext raster_;
  SkiaPulseBenchmark benchmark_{"CPU / Raster + 2D upload"};
};
