#include "utils/fake_mouse_actions.h"

#include <cassert>
#include <cmath>

int main() {
  input::FakeMouseActionsOptions options;
  options.edge_inset = 10.0F;
  options.maximum_drag_distance = 24.0F;
  options.idle_frames_between_drags = 0;
  options.seed = 7U;
  input::FakeMouseActions actions(SkRect::MakeWH(120.0F, 80.0F), options);

  input::FakeMouseFrame frame = actions.Advance(5.0F);
  assert(frame.began && frame.in_drag && !frame.ended);
  const SkPoint start = frame.start;
  frame = actions.Advance(5.0F);
  assert(!frame.began && frame.in_drag);
  assert(std::abs(std::hypot(frame.current.fX - start.fX,
                             frame.current.fY - start.fY) -
                  5.0F) < 0.001F);
  while (!frame.ended) {
    frame = actions.Advance(5.0F);
  }
  assert(frame.current == frame.end);
  assert(std::hypot(frame.end.fX - frame.start.fX,
                    frame.end.fY - frame.start.fY) <= 24.001F);
  assert(actions.Advance(5.0F).began);

  input::FakeMouseActionsOptions mixed_options;
  mixed_options.edge_inset = 10.0F;
  mixed_options.maximum_drag_distance = 30.0F;
  mixed_options.idle_frames_between_drags = 0;
  mixed_options.seed = 19U;
  input::FakeMouseActions mixed(SkRect::MakeWH(200.0F, 100.0F), mixed_options);
  const SkPoint bounds_center = {100.0F, 50.0F};
  int inward_count = 0;
  int outward_count = 0;
  int center_to_center_count = 0;
  int sideways_count = 0;
  int diagonal_count = 0;
  for (int drag = 0; drag < 256; ++drag) {
    const input::FakeMouseFrame began = mixed.Advance(1000.0F);
    assert(began.began && began.in_drag);
    const input::FakeMouseFrame ended = mixed.Advance(1000.0F);
    assert(ended.ended && ended.current == ended.end);
    const float dx = ended.end.fX - ended.start.fX;
    const float dy = ended.end.fY - ended.start.fY;
    assert(std::hypot(dx, dy) <= 30.001F);
    const float start_radius = std::hypot(ended.start.fX - bounds_center.fX,
                                          ended.start.fY - bounds_center.fY);
    const float end_radius = std::hypot(ended.end.fX - bounds_center.fX,
                                        ended.end.fY - bounds_center.fY);
    const bool start_on_edge = std::abs(ended.start.fX - 10.0F) < 0.001F ||
                               std::abs(ended.start.fX - 190.0F) < 0.001F ||
                               std::abs(ended.start.fY - 10.0F) < 0.001F ||
                               std::abs(ended.start.fY - 90.0F) < 0.001F;
    const bool start_near_center =
        ended.start.fX >= 59.0F && ended.start.fX <= 141.0F &&
        ended.start.fY >= 32.0F && ended.start.fY <= 68.0F;
    const bool end_near_center = ended.end.fX >= 59.0F &&
                                 ended.end.fX <= 141.0F &&
                                 ended.end.fY >= 32.0F && ended.end.fY <= 68.0F;
    if (start_on_edge && end_radius < start_radius) {
      ++inward_count;
    }
    if (start_near_center && end_radius > start_radius + 1.0F) {
      ++outward_count;
    }
    if (start_near_center && end_near_center) {
      ++center_to_center_count;
    }
    if (std::abs(dx) > std::abs(dy) * 3.0F ||
        std::abs(dy) > std::abs(dx) * 3.0F) {
      ++sideways_count;
    }
    if (std::abs(dx) > 3.0F && std::abs(dy) > 3.0F) {
      ++diagonal_count;
    }
  }
  assert(inward_count > 35);
  assert(outward_count > 35);
  assert(center_to_center_count > 25);
  assert(sideways_count > 35);
  assert(diagonal_count > 80);

  // Explicit weights can select an outward-only sequence. Pointer-down begins
  // in the central region and every pointer-up moves farther from center.
  input::FakeMouseActionsOptions outward_options = mixed_options;
  outward_options.inward_drag_weight = 0.0F;
  outward_options.outward_drag_weight = 1.0F;
  outward_options.center_drag_weight = 0.0F;
  outward_options.sideways_drag_weight = 0.0F;
  input::FakeMouseActions outward(SkRect::MakeWH(200.0F, 100.0F),
                                  outward_options);
  for (int drag = 0; drag < 16; ++drag) {
    const input::FakeMouseFrame began = outward.Advance(1000.0F);
    const input::FakeMouseFrame ended = outward.Advance(1000.0F);
    const float start_radius = std::hypot(began.start.fX - bounds_center.fX,
                                          began.start.fY - bounds_center.fY);
    const float end_radius = std::hypot(ended.end.fX - bounds_center.fX,
                                        ended.end.fY - bounds_center.fY);
    assert(end_radius > start_radius);
    assert(ended.ended);
  }
  return 0;
}
