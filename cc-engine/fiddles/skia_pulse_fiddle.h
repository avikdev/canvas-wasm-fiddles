#pragma once

#include <memory>

#include "fiddle_base.h"

class GraphiteCanvasContext;

class SkiaPulseFiddle final : public FiddleBase {
 public:
  SkiaPulseFiddle();
  ~SkiaPulseFiddle() override;

  void Render(double time_seconds) override;

 protected:
  bool UsesWebGpu() const override;

 private:
  bool EnsureGraphite();
  void RecordTimings(double acquire_ms, double draw_ms, double submit_ms);

  std::unique_ptr<GraphiteCanvasContext> graphite_;
  int timing_frame_count_ = 0;
  double accumulated_acquire_ms_ = 0.0;
  double accumulated_draw_ms_ = 0.0;
  double accumulated_submit_ms_ = 0.0;
};
