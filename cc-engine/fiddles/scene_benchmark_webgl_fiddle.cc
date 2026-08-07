#include "fiddles/scene_benchmark_webgl_fiddle.h"

#include <chrono>
#include <iostream>

#include "fiddles/scene_benchmark_builder.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkSurface.h"

namespace {

using TimingClock = std::chrono::steady_clock;

double ElapsedMilliseconds(TimingClock::time_point start,
                           TimingClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

SceneBenchmarkWebGlFiddle::SceneBenchmarkWebGlFiddle() = default;

SceneBenchmarkWebGlFiddle::~SceneBenchmarkWebGlFiddle() = default;

bool SceneBenchmarkWebGlFiddle::EnsureWebGl() {
  if (webgl_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

void SceneBenchmarkWebGlFiddle::Render(double time_seconds) {
  if (!EnsureWebGl()) {
    return;
  }

  const int width = PixelWidth();
  const int height = PixelHeight();
  if (!UpdateState(time_seconds, width, height)) {
    return;
  }

  const auto surface_start = TimingClock::now();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
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
  const WebGlPresentResult present = webgl_->FlushAndPresent();
  const double present_ms =
      ElapsedMilliseconds(present_start, TimingClock::now());
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] WebGL could not submit the Skia frame."
              << std::endl;
    return;
  }

  benchmark_.Record({.surface_ms = surface_ms,
                     .scene_draw_ms = scene_draw_ms,
                     .present_ms = present_ms,
                     .flush_ms = present.flush_ms,
                     .submit_ms = present.submit_ms});
}

bool SceneBenchmarkWebGlFiddle::UpdateState(double time_seconds, int, int) {
  time_seconds_ = time_seconds;
  return true;
}

void SceneBenchmarkWebGlFiddle::DrawFrame(SkCanvas *canvas, int width,
                                          int height) {
  DrawSceneBenchmark(canvas, width, height, time_seconds_);
}
