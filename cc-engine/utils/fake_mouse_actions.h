#pragma once

#include <cstdint>
#include <random>

#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

namespace input {

struct FakeMouseActionsOptions {
  float edge_inset = 12.0F;
  // Zero uses 70% of the bounds' shorter side.
  float maximum_drag_distance = 0.0F;
  // Gesture-family weights. They are normalized internally, so callers may
  // expose them directly as relative UI controls.
  float inward_drag_weight = 0.30F;
  float outward_drag_weight = 0.30F;
  float center_drag_weight = 0.25F;
  float sideways_drag_weight = 0.15F;
  // Width and height of the centered sampling region, relative to the usable
  // bounds. Values are clamped to [0.1, 0.9].
  float center_region_ratio = 0.45F;
  // Shortest generated gesture relative to maximum_drag_distance.
  float minimum_drag_length_ratio = 0.45F;
  int idle_frames_between_drags = 8;
  std::uint32_t seed = 0x4d455348U;
};

struct FakeMouseFrame {
  SkPoint start = {0.0F, 0.0F};
  SkPoint current = {0.0F, 0.0F};
  SkPoint end = {0.0F, 0.0F};
  bool in_drag = false;
  bool began = false;
  bool ended = false;
  std::uint64_t drag_index = 0U;
};

// Generates deterministic straight drags from four gesture families: edge to
// interior, interior to edge, center to center, and predominantly sideways.
// Advance() moves exactly step_distance unless the endpoint is closer.
class FakeMouseActions {
public:
  explicit FakeMouseActions(const SkRect &bounds,
                            const FakeMouseActionsOptions &options = {});

  void SetBounds(const SkRect &bounds);
  FakeMouseFrame Advance(float step_distance);

private:
  void BeginNextDrag();
  float Random(float minimum, float maximum);

  SkRect bounds_;
  FakeMouseActionsOptions options_;
  std::mt19937 random_;
  SkPoint start_ = {0.0F, 0.0F};
  SkPoint current_ = {0.0F, 0.0F};
  SkPoint end_ = {0.0F, 0.0F};
  int idle_frames_remaining_ = 0;
  std::uint64_t drag_index_ = 0U;
  bool active_ = false;
};

} // namespace input
