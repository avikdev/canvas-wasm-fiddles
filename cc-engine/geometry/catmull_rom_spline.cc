#include "geometry/catmull_rom_spline.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkPathBuilder.h"

namespace geometry {
namespace {

bool IsFinite(const SkPoint &point) {
  return std::isfinite(point.fX) && std::isfinite(point.fY);
}

bool SamePoint(const SkPoint &first, const SkPoint &second) {
  return first.fX == second.fX && first.fY == second.fY;
}

SkPoint ControlFrom(const SkPoint &anchor, const SkPoint &neighbor_delta,
                    float scale) {
  return {anchor.fX + neighbor_delta.fX * scale,
          anchor.fY + neighbor_delta.fY * scale};
}

} // namespace

SkPath CatmullRomToCubicPath(const std::vector<SkPoint> &points,
                             const CatmullRomOptions &options) {
  if (!std::isfinite(options.tension)) {
    return {};
  }

  std::vector<SkPoint> filtered;
  filtered.reserve(points.size());
  for (const SkPoint &point : points) {
    if (!IsFinite(point)) {
      return {};
    }
    if (filtered.empty() || !SamePoint(filtered.back(), point)) {
      filtered.push_back(point);
    }
  }
  if (options.closed && filtered.size() > 1U &&
      SamePoint(filtered.front(), filtered.back())) {
    filtered.pop_back();
  }

  const std::size_t point_count = filtered.size();
  if ((!options.closed && point_count < 2U) ||
      (options.closed && point_count < 3U)) {
    return {};
  }

  const float tangent_scale =
      (1.0F - std::clamp(options.tension, 0.0F, 1.0F)) / 6.0F;
  const std::size_t segment_count =
      options.closed ? point_count : point_count - 1U;

  SkPathBuilder builder;
  builder.moveTo(filtered.front());
  for (std::size_t segment = 0; segment < segment_count; ++segment) {
    const std::size_t first_index = segment;
    const std::size_t second_index = (segment + 1U) % point_count;
    const std::size_t previous_index =
        options.closed ? (first_index + point_count - 1U) % point_count
                       : (first_index == 0U ? first_index : first_index - 1U);
    const std::size_t next_index =
        options.closed ? (second_index + 1U) % point_count
                       : std::min(second_index + 1U, point_count - 1U);

    const SkPoint &first = filtered[first_index];
    const SkPoint &second = filtered[second_index];
    const SkPoint first_delta = {
        second.fX - filtered[previous_index].fX,
        second.fY - filtered[previous_index].fY,
    };
    const SkPoint second_delta = {
        filtered[next_index].fX - first.fX,
        filtered[next_index].fY - first.fY,
    };
    const SkPoint control1 = ControlFrom(first, first_delta, tangent_scale);
    const SkPoint control2 = ControlFrom(second, second_delta, -tangent_scale);
    builder.cubicTo(control1, control2, second);
  }
  if (options.closed) {
    builder.close();
  }
  return builder.detach();
}

} // namespace geometry
