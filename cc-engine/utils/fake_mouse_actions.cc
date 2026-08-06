#include "utils/fake_mouse_actions.h"

#include <algorithm>
#include <cmath>

namespace input {

FakeMouseActions::FakeMouseActions(const SkRect &bounds,
                                   const FakeMouseActionsOptions &options)
    : bounds_(bounds), options_(options), random_(options.seed) {}

void FakeMouseActions::SetBounds(const SkRect &bounds) {
  bounds_ = bounds;
  active_ = false;
  idle_frames_remaining_ = 0;
}

FakeMouseFrame FakeMouseActions::Advance(float step_distance) {
  FakeMouseFrame frame;
  frame.drag_index = drag_index_;
  if (bounds_.isEmpty() || !bounds_.isFinite() ||
      !std::isfinite(step_distance) || step_distance <= 0.0F) {
    return frame;
  }
  if (!active_) {
    if (idle_frames_remaining_ > 0) {
      --idle_frames_remaining_;
      return frame;
    }
    BeginNextDrag();
    frame = {.start = start_,
             .current = current_,
             .end = end_,
             .in_drag = true,
             .began = true,
             .ended = false,
             .drag_index = drag_index_};
    return frame;
  }

  const float dx = end_.fX - current_.fX;
  const float dy = end_.fY - current_.fY;
  const float remaining = std::hypot(dx, dy);
  bool ended = remaining <= step_distance;
  if (ended) {
    current_ = end_;
  } else {
    current_.fX += dx * step_distance / remaining;
    current_.fY += dy * step_distance / remaining;
  }
  frame = {.start = start_,
           .current = current_,
           .end = end_,
           .in_drag = true,
           .began = false,
           .ended = ended,
           .drag_index = drag_index_};
  if (ended) {
    active_ = false;
    idle_frames_remaining_ = std::max(0, options_.idle_frames_between_drags);
  }
  return frame;
}

void FakeMouseActions::BeginNextDrag() {
  const float inset = std::clamp(
      options_.edge_inset, 0.0F,
      std::max(0.0F, std::min(bounds_.width(), bounds_.height()) * 0.25F));
  const float left = bounds_.left() + inset;
  const float right = bounds_.right() - inset;
  const float top = bounds_.top() + inset;
  const float bottom = bounds_.bottom() - inset;
  const float default_extent =
      std::min(bounds_.width(), bounds_.height()) * 0.70F;
  const float maximum_extent = std::isfinite(options_.maximum_drag_distance) &&
                                       options_.maximum_drag_distance > 0.0F
                                   ? options_.maximum_drag_distance
                                   : default_extent;
  const float minimum_length_ratio =
      std::clamp(options_.minimum_drag_length_ratio, 0.05F, 1.0F);
  const float length =
      Random(maximum_extent * minimum_length_ratio, maximum_extent);
  const float center_ratio =
      std::clamp(options_.center_region_ratio, 0.1F, 0.9F);
  const float center_left =
      std::lerp(left, right, (1.0F - center_ratio) * 0.5F);
  const float center_right =
      std::lerp(left, right, (1.0F + center_ratio) * 0.5F);
  const float center_top = std::lerp(top, bottom, (1.0F - center_ratio) * 0.5F);
  const float center_bottom =
      std::lerp(top, bottom, (1.0F + center_ratio) * 0.5F);

  const auto center_point = [&]() {
    return SkPoint{Random(center_left, center_right),
                   Random(center_top, center_bottom)};
  };
  const auto edge_point = [&](int side) {
    switch (side) {
    case 0:
      return SkPoint{Random(left, right), top};
    case 1:
      return SkPoint{Random(left, right), bottom};
    case 2:
      return SkPoint{left, Random(top, bottom)};
    default:
      return SkPoint{right, Random(top, bottom)};
    }
  };
  const auto aim_toward = [&](const SkPoint &from, const SkPoint &target) {
    const float dx = target.fX - from.fX;
    const float dy = target.fY - from.fY;
    const float distance = std::hypot(dx, dy);
    if (distance <= 0.0001F) {
      return from;
    }
    const float used_length = std::min(length, distance);
    return SkPoint{from.fX + dx * used_length / distance,
                   from.fY + dy * used_length / distance};
  };

  const float inward_weight = std::max(0.0F, options_.inward_drag_weight);
  const float outward_weight = std::max(0.0F, options_.outward_drag_weight);
  const float center_weight = std::max(0.0F, options_.center_drag_weight);
  const float sideways_weight = std::max(0.0F, options_.sideways_drag_weight);
  const float total_weight =
      inward_weight + outward_weight + center_weight + sideways_weight;
  const float family = total_weight > 0.0F ? Random(0.0F, total_weight) : 0.0F;

  if (total_weight <= 0.0F || family < inward_weight) {
    start_ = edge_point(static_cast<int>(random_() % 4U));
    end_ = aim_toward(start_, center_point());
  } else if (family < inward_weight + outward_weight) {
    start_ = center_point();
    const SkPoint bounds_center = {bounds_.centerX(), bounds_.centerY()};
    const bool horizontal = random_() % 2U == 0U;
    const SkPoint outward_target =
        horizontal
            ? SkPoint{start_.fX < bounds_center.fX ? left : right, start_.fY}
            : SkPoint{start_.fX, start_.fY < bounds_center.fY ? top : bottom};
    end_ = aim_toward(start_, outward_target);
  } else if (family < inward_weight + outward_weight + center_weight) {
    start_ = center_point();
    SkPoint target = center_point();
    for (int attempt = 0;
         attempt < 8 &&
         std::hypot(target.fX - start_.fX, target.fY - start_.fY) <
             maximum_extent * minimum_length_ratio * 0.5F;
         ++attempt) {
      target = center_point();
    }
    end_ = aim_toward(start_, target);
  } else {
    const bool horizontal = random_() % 2U == 0U;
    const float sign = random_() % 2U == 0U ? -1.0F : 1.0F;
    start_ = center_point();
    const SkPoint target =
        horizontal ? SkPoint{sign < 0.0F ? left : right,
                             start_.fY + Random(-0.08F, 0.08F) * (bottom - top)}
                   : SkPoint{start_.fX + Random(-0.08F, 0.08F) * (right - left),
                             sign < 0.0F ? top : bottom};
    end_ = aim_toward(start_, target);
  }
  current_ = start_;
  active_ = true;
  ++drag_index_;
}

float FakeMouseActions::Random(float minimum, float maximum) {
  if (maximum <= minimum) {
    return minimum;
  }
  return std::uniform_real_distribution<float>(minimum, maximum)(random_);
}

} // namespace input
