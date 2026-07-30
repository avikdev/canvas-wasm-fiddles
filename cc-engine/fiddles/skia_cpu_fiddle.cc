#include "fiddles/skia_cpu_fiddle.h"

#include <chrono>

#include "fiddles/skia_pulse_scene.h"
#include "include/core/SkSurface.h"

namespace {

using TimingClock = std::chrono::steady_clock;

double ElapsedMilliseconds(TimingClock::time_point start,
                           TimingClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

void SkiaCpuFiddle::Render(double time_seconds) {
  const int width = Canvas()["width"].as<int>();
  const int height = Canvas()["height"].as<int>();

  const auto surface_start = TimingClock::now();
  SkSurface *surface = raster_.AcquireSurface(width, height);
  const double surface_ms =
      ElapsedMilliseconds(surface_start, TimingClock::now());
  if (surface == nullptr) {
    return;
  }

  const auto scene_start = TimingClock::now();
  DrawSkiaPulseScene(surface->getCanvas(), width, height, time_seconds);
  const double scene_draw_ms =
      ElapsedMilliseconds(scene_start, TimingClock::now());

  const auto present_start = TimingClock::now();
  raster_.Present(Context());
  const double present_ms =
      ElapsedMilliseconds(present_start, TimingClock::now());

  benchmark_.Record({.surface_ms = surface_ms,
                     .scene_draw_ms = scene_draw_ms,
                     .present_ms = present_ms});
}
