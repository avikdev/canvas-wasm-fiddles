#include "fiddles/scene_pulse_benchmark.h"

#include <iomanip>
#include <iostream>
#include <sstream>

ScenePulseBenchmark::ScenePulseBenchmark(std::string_view backend_label)
    : backend_label_(backend_label) {}

void ScenePulseBenchmark::Record(const ScenePulseFrameTimings &timings) {
  accumulated_.surface_ms += timings.surface_ms;
  accumulated_.scene_draw_ms += timings.scene_draw_ms;
  accumulated_.present_ms += timings.present_ms;
  accumulated_.flush_ms += timings.flush_ms;
  accumulated_.submit_ms += timings.submit_ms;
  ++frame_count_;

  if (frame_count_ < kWindowFrames) {
    return;
  }

  const double total_ms = accumulated_.surface_ms + accumulated_.scene_draw_ms +
                          accumulated_.present_ms;
  std::ostringstream report;
  report << std::fixed << std::setprecision(2)
         << "[cc-engine/stdout] Scene benchmark [" << backend_label_
         << "] last " << kWindowFrames
         << " frames (accumulated ms): surface=" << accumulated_.surface_ms
         << ", scene-draw=" << accumulated_.scene_draw_ms
         << ", present=" << accumulated_.present_ms
         << ", measured-total=" << total_ms
         << ", avg/frame=" << total_ms / kWindowFrames;
  if (accumulated_.flush_ms > 0.0 || accumulated_.submit_ms > 0.0) {
    report << ", present-breakdown(flush=" << accumulated_.flush_ms
           << ", submit=" << accumulated_.submit_ms << ")";
  }
  std::cout << report.str() << std::endl;

  frame_count_ = 0;
  accumulated_ = {};
}
