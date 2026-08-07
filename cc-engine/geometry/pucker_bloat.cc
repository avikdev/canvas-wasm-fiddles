#include "geometry/pucker_bloat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "include/core/SkPathBuilder.h"

namespace geometry {
namespace {

constexpr float kEpsilon = 0.00001F;

struct Cubic {
  SkPoint start;
  SkPoint control1;
  SkPoint control2;
  SkPoint end;
};

struct SplitCubic {
  Cubic left;
  Cubic right;
  SkPoint midpoint;
};

SkPoint Lerp(const SkPoint &from, const SkPoint &to, float amount) {
  return {std::lerp(from.fX, to.fX, amount), std::lerp(from.fY, to.fY, amount)};
}

SkPoint Evaluate(const Cubic &cubic, float parameter) {
  const float inverse = 1.0F - parameter;
  return {
      inverse * inverse * inverse * cubic.start.fX +
          3.0F * inverse * inverse * parameter * cubic.control1.fX +
          3.0F * inverse * parameter * parameter * cubic.control2.fX +
          parameter * parameter * parameter * cubic.end.fX,
      inverse * inverse * inverse * cubic.start.fY +
          3.0F * inverse * inverse * parameter * cubic.control1.fY +
          3.0F * inverse * parameter * parameter * cubic.control2.fY +
          parameter * parameter * parameter * cubic.end.fY,
  };
}

float Distance(const SkPoint &first, const SkPoint &second) {
  return std::hypot(second.fX - first.fX, second.fY - first.fY);
}

std::optional<SkPoint> AreaCentroid(const SkPath &path) {
  constexpr int curve_sample_count = 16;
  double twice_area_sum = 0.0;
  double weighted_x_sum = 0.0;
  double weighted_y_sum = 0.0;
  std::vector<SkPoint> contour;

  const auto finish_contour = [&]() {
    if (contour.size() >= 3U) {
      for (std::size_t index = 0; index < contour.size(); ++index) {
        const SkPoint &first = contour[index];
        const SkPoint &second = contour[(index + 1U) % contour.size()];
        const double cross = static_cast<double>(first.fX) * second.fY -
                             static_cast<double>(second.fX) * first.fY;
        twice_area_sum += cross;
        weighted_x_sum += (static_cast<double>(first.fX) + second.fX) * cross;
        weighted_y_sum += (static_cast<double>(first.fY) + second.fY) * cross;
      }
    }
    contour.clear();
  };
  const auto append_samples = [&](const auto &evaluate) {
    for (int sample = 1; sample <= curve_sample_count; ++sample) {
      contour.push_back(
          evaluate(static_cast<float>(sample) / curve_sample_count));
    }
  };

  SkPath::RawIter iterator(path);
  SkPoint points[4];
  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    switch (verb) {
    case SkPath::kMove_Verb:
      finish_contour();
      contour.push_back(points[0]);
      break;
    case SkPath::kLine_Verb:
      contour.push_back(points[1]);
      break;
    case SkPath::kQuad_Verb:
      append_samples([&](float parameter) {
        const float inverse = 1.0F - parameter;
        return SkPoint{inverse * inverse * points[0].fX +
                           2.0F * inverse * parameter * points[1].fX +
                           parameter * parameter * points[2].fX,
                       inverse * inverse * points[0].fY +
                           2.0F * inverse * parameter * points[1].fY +
                           parameter * parameter * points[2].fY};
      });
      break;
    case SkPath::kConic_Verb: {
      const float weight = iterator.conicWeight();
      append_samples([&](float parameter) {
        const float inverse = 1.0F - parameter;
        const float denominator = inverse * inverse +
                                  2.0F * weight * inverse * parameter +
                                  parameter * parameter;
        return SkPoint{(inverse * inverse * points[0].fX +
                        2.0F * weight * inverse * parameter * points[1].fX +
                        parameter * parameter * points[2].fX) /
                           denominator,
                       (inverse * inverse * points[0].fY +
                        2.0F * weight * inverse * parameter * points[1].fY +
                        parameter * parameter * points[2].fY) /
                           denominator};
      });
      break;
    }
    case SkPath::kCubic_Verb: {
      const Cubic cubic = {points[0], points[1], points[2], points[3]};
      append_samples(
          [&](float parameter) { return Evaluate(cubic, parameter); });
      break;
    }
    case SkPath::kClose_Verb:
      finish_contour();
      break;
    case SkPath::kDone_Verb:
      break;
    }
  }
  finish_contour();
  if (!std::isfinite(twice_area_sum) ||
      std::abs(twice_area_sum) <= static_cast<double>(kEpsilon)) {
    return std::nullopt;
  }
  return SkPoint{static_cast<float>(weighted_x_sum / (3.0 * twice_area_sum)),
                 static_cast<float>(weighted_y_sum / (3.0 * twice_area_sum))};
}

float HalfArcLengthParameter(const Cubic &cubic, int requested_sample_count) {
  const int sample_count = std::clamp(requested_sample_count, 8, 256);
  std::vector<float> cumulative(static_cast<std::size_t>(sample_count) + 1U,
                                0.0F);
  SkPoint previous = cubic.start;
  for (int sample = 1; sample <= sample_count; ++sample) {
    const float parameter =
        static_cast<float>(sample) / static_cast<float>(sample_count);
    const SkPoint point = Evaluate(cubic, parameter);
    cumulative[static_cast<std::size_t>(sample)] =
        cumulative[static_cast<std::size_t>(sample - 1)] +
        Distance(previous, point);
    previous = point;
  }
  const float total = cumulative.back();
  if (total <= kEpsilon) {
    return 0.5F;
  }
  const float target = total * 0.5F;
  const auto upper =
      std::lower_bound(cumulative.begin(), cumulative.end(), target);
  const std::size_t upper_index =
      static_cast<std::size_t>(std::distance(cumulative.begin(), upper));
  if (upper_index == 0U || upper_index >= cumulative.size()) {
    return 0.5F;
  }
  const float lower_length = cumulative[upper_index - 1U];
  const float interval_length = cumulative[upper_index] - lower_length;
  const float interval_fraction =
      interval_length <= kEpsilon ? 0.0F
                                  : (target - lower_length) / interval_length;
  return (static_cast<float>(upper_index - 1U) + interval_fraction) /
         static_cast<float>(sample_count);
}

SplitCubic Split(const Cubic &cubic, float parameter) {
  const SkPoint first = Lerp(cubic.start, cubic.control1, parameter);
  const SkPoint second = Lerp(cubic.control1, cubic.control2, parameter);
  const SkPoint third = Lerp(cubic.control2, cubic.end, parameter);
  const SkPoint fourth = Lerp(first, second, parameter);
  const SkPoint fifth = Lerp(second, third, parameter);
  const SkPoint midpoint = Lerp(fourth, fifth, parameter);
  return {
      .left = {cubic.start, first, fourth, midpoint},
      .right = {midpoint, fifth, third, cubic.end},
      .midpoint = midpoint,
  };
}

SkPoint RadialScale(const SkPoint &point, const SkPoint &pivot, float scale) {
  return {pivot.fX + (point.fX - pivot.fX) * scale,
          pivot.fY + (point.fY - pivot.fY) * scale};
}

SkPoint FixedDistanceToward(const SkPoint &origin, const SkPoint &target,
                            float distance, const SkPoint &fallback_vector) {
  float direction_x = target.fX - origin.fX;
  float direction_y = target.fY - origin.fY;
  float length = std::hypot(direction_x, direction_y);
  if (length <= kEpsilon) {
    direction_x = fallback_vector.fX;
    direction_y = fallback_vector.fY;
    length = std::hypot(direction_x, direction_y);
  }
  if (length <= kEpsilon || distance <= kEpsilon) {
    return origin;
  }
  return {origin.fX + direction_x * distance / length,
          origin.fY + direction_y * distance / length};
}

bool SamePoint(const SkPoint &first, const SkPoint &second) {
  return Distance(first, second) <= kEpsilon;
}

void AddUnique(std::vector<SkPoint> *points, const SkPoint &point) {
  if (points->empty() || !SamePoint(points->back(), point)) {
    points->push_back(point);
  }
}

class SplitSegmentAlgorithm final : public PuckerBloatAlgorithm {
public:
  PuckerBloatResult Apply(const SkPath &input_path, const SkPoint &pivot,
                          float amount,
                          const PuckerBloatOptions &options) const override {
    PuckerBloatResult result;
    result.pivot = pivot;
    SkPathBuilder builder;
    builder.setFillType(input_path.getFillType());

    const float anchor_scale = 1.0F - amount;
    const float midpoint_scale = 1.0F + amount;
    SkPoint contour_start = {0.0F, 0.0F};
    SkPoint current = {0.0F, 0.0F};
    bool contour_open = false;

    const auto emit = [&](const Cubic &cubic) {
      const float split_parameter =
          options.split_by_arc_length
              ? HalfArcLengthParameter(cubic, options.arc_length_sample_count)
              : 0.5F;
      const SplitCubic split = Split(cubic, split_parameter);
      const SkPoint transformed_start =
          RadialScale(cubic.start, pivot, anchor_scale);
      const SkPoint transformed_end =
          RadialScale(cubic.end, pivot, anchor_scale);
      const SkPoint transformed_midpoint =
          RadialScale(split.midpoint, pivot, midpoint_scale);
      const float original_midpoint_distance = Distance(split.midpoint, pivot);
      const float inner_handle_scale =
          original_midpoint_distance > kEpsilon
              ? Distance(transformed_midpoint, pivot) /
                    original_midpoint_distance
              : 1.0F;

      const SkPoint inner_left = {
          transformed_midpoint.fX +
              (split.left.control2.fX - split.midpoint.fX) * inner_handle_scale,
          transformed_midpoint.fY +
              (split.left.control2.fY - split.midpoint.fY) *
                  inner_handle_scale};
      const SkPoint inner_right = {
          transformed_midpoint.fX +
              (split.right.control1.fX - split.midpoint.fX) *
                  inner_handle_scale,
          transformed_midpoint.fY +
              (split.right.control1.fY - split.midpoint.fY) *
                  inner_handle_scale};
      const SkPoint outer_left =
          FixedDistanceToward(transformed_start, split.left.control1,
                              Distance(cubic.start, split.left.control1),
                              {split.left.control1.fX - cubic.start.fX,
                               split.left.control1.fY - cubic.start.fY});
      const SkPoint outer_right =
          FixedDistanceToward(transformed_end, split.right.control2,
                              Distance(cubic.end, split.right.control2),
                              {split.right.control2.fX - cubic.end.fX,
                               split.right.control2.fY - cubic.end.fY});

      builder.cubicTo(outer_left, inner_left, transformed_midpoint);
      builder.cubicTo(inner_right, outer_right, transformed_end);
      AddUnique(&result.anchors, transformed_start);
      AddUnique(&result.anchors, transformed_end);
      result.midpoints.push_back(transformed_midpoint);
    };

    SkPath::RawIter iterator(input_path);
    SkPoint points[4];
    for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
         verb = iterator.next(points)) {
      switch (verb) {
      case SkPath::kMove_Verb:
        contour_start = points[0];
        current = points[0];
        contour_open = true;
        builder.moveTo(RadialScale(current, pivot, anchor_scale));
        break;
      case SkPath::kLine_Verb:
        emit({points[0], Lerp(points[0], points[1], 1.0F / 3.0F),
              Lerp(points[0], points[1], 2.0F / 3.0F), points[1]});
        current = points[1];
        break;
      case SkPath::kQuad_Verb:
        emit({points[0], Lerp(points[0], points[1], 2.0F / 3.0F),
              Lerp(points[2], points[1], 2.0F / 3.0F), points[2]});
        current = points[2];
        break;
      case SkPath::kConic_Verb: {
        const float weight = iterator.conicWeight();
        const float coefficient = std::isfinite(weight) && weight > 0.0F
                                      ? 4.0F * weight / (3.0F * (1.0F + weight))
                                      : 2.0F / 3.0F;
        emit({points[0], Lerp(points[0], points[1], coefficient),
              Lerp(points[2], points[1], coefficient), points[2]});
        current = points[2];
        break;
      }
      case SkPath::kCubic_Verb:
        emit({points[0], points[1], points[2], points[3]});
        current = points[3];
        break;
      case SkPath::kClose_Verb:
        if (contour_open && !SamePoint(current, contour_start)) {
          emit({current, Lerp(current, contour_start, 1.0F / 3.0F),
                Lerp(current, contour_start, 2.0F / 3.0F), contour_start});
        }
        builder.close();
        current = contour_start;
        contour_open = false;
        break;
      case SkPath::kDone_Verb:
        break;
      }
    }
    result.path = builder.detach();
    return result;
  }
};

} // namespace

const PuckerBloatAlgorithm &SplitSegmentPuckerBloatAlgorithm() {
  static const SplitSegmentAlgorithm algorithm;
  return algorithm;
}

PuckerBloatResult PuckerBloatDetailed(const SkPath &input_path,
                                      const PuckerBloatOptions &options,
                                      const PuckerBloatAlgorithm *algorithm) {
  PuckerBloatResult unchanged;
  unchanged.path = input_path;
  const SkRect bounds = input_path.getBounds();
  if (input_path.isEmpty() || !bounds.isFinite() ||
      bounds.width() <= kEpsilon || bounds.height() <= kEpsilon ||
      !std::isfinite(options.amount) ||
      !std::isfinite(options.displacement_scale)) {
    return unchanged;
  }
  const SkPoint pivot =
      options.pivot_mode == PuckerBloatPivotMode::kCustomPoint
          ? options.custom_pivot
          : AreaCentroid(input_path)
                .value_or(SkPoint{bounds.centerX(), bounds.centerY()});
  if (!pivot.isFinite()) {
    return unchanged;
  }
  const float amount = std::clamp(options.amount, -1.0F, 1.0F) *
                       std::clamp(options.displacement_scale, 0.0F, 1.0F);
  const PuckerBloatAlgorithm &selected =
      algorithm == nullptr ? SplitSegmentPuckerBloatAlgorithm() : *algorithm;
  PuckerBloatResult result = selected.Apply(input_path, pivot, amount, options);
  result.pivot = pivot;
  if (std::abs(amount) <= kEpsilon) {
    result.path = input_path;
  }
  return result;
}

SkPath PuckerBloat(const SkPath &input_path,
                   const PuckerBloatOptions &options) {
  return PuckerBloatDetailed(input_path, options).path;
}

} // namespace geometry
