#include "fiddles/scene_benchmark_cpu_fiddle.h"

#include <chrono>

#include "fiddles/scene_benchmark_builder.h"
#include "include/core/SkSurface.h"

namespace {

using TimingClock = std::chrono::steady_clock;

double ElapsedMilliseconds(TimingClock::time_point start,
                           TimingClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

void SceneBenchmarkCpuFiddle::Render(double time_seconds) {
  const int width = PixelWidth();
  const int height = PixelHeight();
  if (!UpdateState(time_seconds, width, height)) {
    return;
  }

  const auto surface_start = TimingClock::now();
  SkSurface *surface = raster_.AcquireSurface(width, height);
  const double surface_ms =
      ElapsedMilliseconds(surface_start, TimingClock::now());
  if (surface == nullptr) {
    return;
  }

  const auto scene_start = TimingClock::now();
  DrawFrame(surface->getCanvas(), width, height);
  const double scene_draw_ms =
      ElapsedMilliseconds(scene_start, TimingClock::now());

  const auto present_start = TimingClock::now();
  raster_.Present(CpuResource());
  const double present_ms =
      ElapsedMilliseconds(present_start, TimingClock::now());

  benchmark_.Record({.surface_ms = surface_ms,
                     .scene_draw_ms = scene_draw_ms,
                     .present_ms = present_ms});
}

bool SceneBenchmarkCpuFiddle::UpdateState(double time_seconds, int, int) {
  time_seconds_ = time_seconds;
  return true;
}

void SceneBenchmarkCpuFiddle::DrawFrame(SkCanvas *canvas, int width,
                                        int height) {
  DrawSceneBenchmark(canvas, width, height, time_seconds_);
}
