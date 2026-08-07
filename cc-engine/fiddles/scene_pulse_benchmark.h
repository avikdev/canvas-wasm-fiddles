#pragma once

#include <string_view>

struct ScenePulseFrameTimings {
  double surface_ms = 0.0;
  double scene_draw_ms = 0.0;
  double present_ms = 0.0;
  double flush_ms = 0.0;
  double submit_ms = 0.0;
};

class ScenePulseBenchmark final {
public:
  explicit ScenePulseBenchmark(std::string_view backend_label);

  void Record(const ScenePulseFrameTimings &timings);

private:
  static constexpr int kWindowFrames = 120;

  std::string_view backend_label_;
  int frame_count_ = 0;
  ScenePulseFrameTimings accumulated_;
};
