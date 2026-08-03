#include "morphing/shape_morpher.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "include/core/SkMatrix.h"
#include "include/core/SkPathBuilder.h"
#include "morphing/correspondence_solver.h"
#include "morphing/geometry_kernel.h"
#include "morphing/path_geometry.h"
#include "morphing/topology_guard.h"

namespace skmorph {
namespace {

struct CubicPair {
  geometry::Cubic source;
  geometry::Cubic target;
};

struct PreparedContour {
  std::vector<CubicPair> segments;
  SkPoint source_original_start = {0.0F, 0.0F};
  SkPoint target_original_start = {0.0F, 0.0F};
  SkPoint source_shifted_start = {0.0F, 0.0F};
  SkPoint target_shifted_start = {0.0F, 0.0F};
  bool source_present = true;
  bool target_present = true;
  bool relative_to_outer = false;
  SkPoint source_origin = {0.0F, 0.0F};
  SkPoint target_origin = {0.0F, 0.0F};
  float required_clearance = 0.0F;
};

SkPoint LerpPoint(SkPoint source, SkPoint target, float t) {
  return {std::lerp(source.x(), target.x(), t),
          std::lerp(source.y(), target.y(), t)};
}

bool IsFinitePoint(SkPoint point) {
  return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool ValidateOptions(const MorphOptions &options, std::string *error) {
  const auto reject = [&](const char *message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (!std::isfinite(options.sourceRotationDegrees) ||
      !std::isfinite(options.targetRotationDegrees)) {
    return reject("Rotation values must be finite.");
  }
  if (options.startPointMode == StartPointMode::kUserSpecified &&
      (!options.sourceStartPoint.has_value() ||
       !options.targetStartPoint.has_value() ||
       !IsFinitePoint(*options.sourceStartPoint) ||
       !IsFinitePoint(*options.targetStartPoint))) {
    return reject("User-specified mode requires two finite start points.");
  }
  if (!std::isfinite(options.flatnessEpsilon) ||
      options.flatnessEpsilon < 0.0F ||
      !std::isfinite(options.tangentAngleEpsilonDegrees) ||
      options.tangentAngleEpsilonDegrees < 0.0F ||
      !std::isfinite(options.curvatureEpsilon) ||
      options.curvatureEpsilon < 0.0F ||
      !std::isfinite(options.arcLengthEpsilon) ||
      options.arcLengthEpsilon < 0.0F ||
      !std::isfinite(options.turningAngleDegrees) ||
      options.turningAngleDegrees < 0.0F ||
      !std::isfinite(options.maximumSegmentArcLength) ||
      options.maximumSegmentArcLength < 0.0F ||
      !std::isfinite(options.holeClearanceEpsilon) ||
      options.holeClearanceEpsilon < 0.0F) {
    return reject("Subdivision tolerances must be finite and non-negative.");
  }
  if (((options.subdivisionFlags & kSplitMaxArcLength) != 0U ||
       options.enableFeaturePivotCorrespondence) &&
      options.maximumSegmentArcLength <= 0.0F) {
    return reject(
        "maximumSegmentArcLength must be positive when its split flag is "
        "enabled.");
  }
  if (options.maximumSubdivisionDepth < 0 ||
      options.maximumSubdivisionDepth > 24) {
    return reject("maximumSubdivisionDepth must be between 0 and 24.");
  }
  if (options.maximumSegmentsPerContour < 1 ||
      options.maximumSegmentsPerContour > (1 << 20)) {
    return reject("maximumSegmentsPerContour must be between 1 and 1,048,576.");
  }
  return true;
}

SkMatrix RotationForPath(const SkPath &path, float degrees) {
  const SkRect bounds = path.getBounds();
  SkMatrix matrix;
  matrix.setRotate(degrees, bounds.centerX(), bounds.centerY());
  return matrix;
}

SkPath RotatedPath(const SkPath &path, const SkMatrix &rotation) {
  SkPath result;
  path.transform(rotation, &result);
  return result;
}

SkPoint MappedPoint(const SkMatrix &matrix, SkPoint point) {
  return matrix.mapPoint(point);
}

void AlignContourStarts(internal::Contour *source, internal::Contour *target,
                        StartPointMode mode,
                        const std::optional<SkPoint> &source_start,
                        const std::optional<SkPoint> &target_start) {
  if (source->signed_area() * target->signed_area() < 0.0F) {
    target->Reverse();
  }
  if (mode == StartPointMode::kFirstVerb) {
    return;
  }
  if (mode == StartPointMode::kUserSpecified && source_start.has_value() &&
      target_start.has_value()) {
    source->RotateStart(source->NearestPosition(*source_start));
    target->RotateStart(target->NearestPosition(*target_start));
    return;
  }
  SkVector direction = target->center() - source->center();
  if (!direction.normalize()) {
    direction = {1.0F, 0.0F};
  }
  source->RotateStart(source->ExtremePosition(direction));
  target->RotateStart(target->ExtremePosition(direction));
}

bool PassesAdaptiveTests(const geometry::Curve &curve,
                         const MorphOptions &options) {
  const uint32_t flags = options.subdivisionFlags;
  if ((flags & kSplitFlatness) != 0U &&
      curve.Flatness() > options.flatnessEpsilon) {
    return false;
  }
  if ((flags & kSplitTangentAngle) != 0U &&
      curve.EndpointTangentAngleDegrees() >
          options.tangentAngleEpsilonDegrees) {
    return false;
  }
  if ((flags & kSplitCurvature) != 0U &&
      curve.CurvatureVariation() > options.curvatureEpsilon) {
    return false;
  }
  if ((flags & kSplitArcLengthError) != 0U &&
      curve.ArcLengthError(std::max(1e-5F, options.arcLengthEpsilon * 0.1F)) >
          options.arcLengthEpsilon) {
    return false;
  }
  if ((flags & kSplitTurningAngle) != 0U &&
      curve.ControlPolygonTurningAngleDegrees() > options.turningAngleDegrees) {
    return false;
  }
  if ((flags & kSplitMaxArcLength) != 0U &&
      curve.Length(std::max(0.001F, options.maximumSegmentArcLength * 0.001F)) >
          options.maximumSegmentArcLength) {
    return false;
  }
  return true;
}

bool PassesRevisionAdaptiveTests(const geometry::Curve &curve,
                                 const MorphOptions &options) {
  if (!PassesAdaptiveTests(curve, options)) {
    return false;
  }
  return curve.Length(
             std::max(0.001F, options.maximumSegmentArcLength * 0.001F)) <=
         options.maximumSegmentArcLength;
}

void MergePositions(std::vector<float> *destination,
                    const std::vector<float> &source) {
  destination->insert(destination->end(), source.begin(), source.end());
  std::sort(destination->begin(), destination->end());
  destination->erase(std::unique(destination->begin(), destination->end(),
                                 [](float first, float second) {
                                   return std::abs(first - second) <= 1e-5F;
                                 }),
                     destination->end());
  if (destination->empty() || destination->front() > 0.0F) {
    destination->insert(destination->begin(), 0.0F);
  }
  if (destination->back() < 1.0F) {
    destination->push_back(1.0F);
  }
}

void SubdivideInterval(const internal::Contour &source,
                       const internal::Contour &target, float start, float end,
                       int depth, int segment_budget,
                       const MorphOptions &options, PreparedContour *prepared) {
  const geometry::Curve source_curve = source.CurveBetween(start, end);
  const geometry::Curve target_curve = target.CurveBetween(start, end);
  const bool at_limit =
      depth >= options.maximumSubdivisionDepth || segment_budget <= 1;
  if (at_limit || (PassesAdaptiveTests(source_curve, options) &&
                   PassesAdaptiveTests(target_curve, options))) {
    prepared->segments.push_back(
        {source_curve.ToCanonicalCubic(), target_curve.ToCanonicalCubic()});
    return;
  }
  const float middle = (start + end) * 0.5F;
  if (middle <= start || middle >= end) {
    prepared->segments.push_back(
        {source_curve.ToCanonicalCubic(), target_curve.ToCanonicalCubic()});
    return;
  }
  const int left_budget = segment_budget / 2;
  SubdivideInterval(source, target, start, middle, depth + 1, left_budget,
                    options, prepared);
  SubdivideInterval(source, target, middle, end, depth + 1,
                    segment_budget - left_budget, options, prepared);
}

PreparedContour PrepareContour(internal::Contour source,
                               internal::Contour target,
                               const MorphOptions &options,
                               StartPointMode start_mode,
                               const std::optional<SkPoint> &source_start,
                               const std::optional<SkPoint> &target_start) {
  PreparedContour prepared;
  prepared.source_original_start = source.spans().front().curve.start();
  prepared.target_original_start = target.spans().front().curve.start();
  AlignContourStarts(&source, &target, start_mode, source_start, target_start);
  prepared.source_shifted_start = source.spans().front().curve.start();
  prepared.target_shifted_start = target.spans().front().curve.start();
  std::vector<float> positions =
      source.MandatoryPositions(options.subdivisionFlags);
  MergePositions(&positions,
                 target.MandatoryPositions(options.subdivisionFlags));

  const int mandatory_interval_count = static_cast<int>(positions.size()) - 1;
  if (mandatory_interval_count > options.maximumSegmentsPerContour) {
    return prepared;
  }
  prepared.segments.reserve(std::min(options.maximumSegmentsPerContour,
                                     static_cast<int>(positions.size()) * 4));
  for (size_t index = 1; index < positions.size(); ++index) {
    if (positions[index] - positions[index - 1U] <= 1e-6F) {
      continue;
    }
    const int remaining_intervals =
        static_cast<int>(positions.size() - index - 1U);
    const int interval_budget = options.maximumSegmentsPerContour -
                                static_cast<int>(prepared.segments.size()) -
                                remaining_intervals;
    SubdivideInterval(source, target, positions[index - 1U], positions[index],
                      0, interval_budget, options, &prepared);
  }
  return prepared;
}

bool ContainsPosition(const std::vector<float> &positions, float value,
                      float epsilon = 2e-4F) {
  return std::any_of(positions.begin(), positions.end(), [&](float position) {
    return std::abs(position - value) <= epsilon;
  });
}

SkPoint PointAt(const internal::Contour &contour, float position) {
  position = std::clamp(position, 0.0F, 1.0F);
  const float distance = position * contour.length();
  const auto &spans = contour.spans();
  const auto found =
      std::lower_bound(spans.begin(), spans.end(), distance,
                       [](const internal::Contour::Span &span, float value) {
                         return span.end_length < value;
                       });
  const auto &span = found == spans.end() ? spans.back() : *found;
  return span.curve.Evaluate(span.arc_lengths.ParameterAtLength(
      std::clamp(distance - span.start_length, 0.0F,
                 span.end_length - span.start_length)));
}

float CurvatureAt(const internal::Contour &contour, float position) {
  position = std::clamp(position, 0.0F, 1.0F);
  const float distance = position * contour.length();
  const auto &spans = contour.spans();
  const auto found =
      std::lower_bound(spans.begin(), spans.end(), distance,
                       [](const internal::Contour::Span &span, float value) {
                         return span.end_length < value;
                       });
  const auto &span = found == spans.end() ? spans.back() : *found;
  return span.curve.Curvature(span.arc_lengths.ParameterAtLength(
      std::clamp(distance - span.start_length, 0.0F,
                 span.end_length - span.start_length)));
}

std::vector<float> RevisionFeaturePositions(
    const internal::Contour &contour, const MorphOptions &options,
    std::vector<float> *axis_extrema, std::vector<float> *curvature_extrema,
    std::vector<float> *long_straights) {
  std::vector<float> positions =
      contour.MandatoryPositions(kSplitInflection | kSplitCusps);
  constexpr int kFeatureSamples = 24;
  for (const internal::Contour::Span &span : contour.spans()) {
    const float span_length = span.end_length - span.start_length;
    const auto normalized_position = [&](float parameter) {
      return (span.start_length +
              span_length *
                  span.arc_lengths.NormalizedLengthAtParameter(parameter)) /
             contour.length();
    };
    if ((span.curve.type() == geometry::Curve::Type::kLine ||
         span.curve.Flatness() <= std::max(0.01F, options.flatnessEpsilon)) &&
        span_length > options.maximumSegmentArcLength) {
      long_straights->push_back(span.start_length / contour.length());
      long_straights->push_back(span.end_length / contour.length());
    }
    float previous_curvature = std::abs(span.curve.Curvature(0.0F));
    for (int sample = 1; sample < kFeatureSamples; ++sample) {
      const float parameter = static_cast<float>(sample) / kFeatureSamples;
      const float before = static_cast<float>(sample - 1) / kFeatureSamples;
      const float after = static_cast<float>(sample + 1) / kFeatureSamples;
      const SkVector previous_derivative = span.curve.Derivative(before);
      const SkVector next_derivative = span.curve.Derivative(after);
      const auto crosses_zero = [](float before_value, float after_value) {
        constexpr float kDerivativeEpsilon = 1e-5F;
        return (before_value < -kDerivativeEpsilon &&
                after_value >= -kDerivativeEpsilon) ||
               (before_value > kDerivativeEpsilon &&
                after_value <= kDerivativeEpsilon);
      };
      if (crosses_zero(previous_derivative.x(), next_derivative.x()) ||
          crosses_zero(previous_derivative.y(), next_derivative.y())) {
        const float position = normalized_position(parameter);
        axis_extrema->push_back(position);
        positions.push_back(position);
      }
      const float current_curvature = std::abs(span.curve.Curvature(parameter));
      const float next_curvature = std::abs(span.curve.Curvature(after));
      if (current_curvature > previous_curvature &&
          current_curvature >= next_curvature) {
        const float position = normalized_position(parameter);
        curvature_extrema->push_back(position);
        positions.push_back(position);
      }
      previous_curvature = current_curvature;
    }
  }
  MergePositions(&positions, *axis_extrema);
  MergePositions(&positions, *curvature_extrema);
  MergePositions(&positions, *long_straights);
  return positions;
}

void SubdivideSingleInterval(const internal::Contour &contour, float start,
                             float end, int depth, int segment_budget,
                             const MorphOptions &options,
                             std::vector<float> *positions) {
  const geometry::Curve curve = contour.CurveBetween(start, end);
  if (depth >= options.maximumSubdivisionDepth || segment_budget <= 1 ||
      PassesRevisionAdaptiveTests(curve, options)) {
    positions->push_back(end);
    return;
  }
  const float middle = (start + end) * 0.5F;
  const int left_budget = segment_budget / 2;
  SubdivideSingleInterval(contour, start, middle, depth + 1, left_budget,
                          options, positions);
  SubdivideSingleInterval(contour, middle, end, depth + 1,
                          segment_budget - left_budget, options, positions);
}

std::vector<float> BuildRevisionPositions(const internal::Contour &contour,
                                          const MorphOptions &options,
                                          int segment_budget,
                                          std::vector<float> *axis_extrema,
                                          std::vector<float> *curvature_extrema,
                                          std::vector<float> *long_straights) {
  const std::vector<float> mandatory = RevisionFeaturePositions(
      contour, options, axis_extrema, curvature_extrema, long_straights);
  if (static_cast<int>(mandatory.size()) - 1 > segment_budget) {
    return {};
  }
  std::vector<float> result = {0.0F};
  for (size_t index = 1; index < mandatory.size(); ++index) {
    const int intervals_left = static_cast<int>(mandatory.size() - index - 1U);
    const int budget =
        segment_budget - static_cast<int>(result.size()) + 1 - intervals_left;
    SubdivideSingleInterval(contour, mandatory[index - 1U], mandatory[index], 0,
                            std::max(1, budget), options, &result);
  }
  MergePositions(&result, mandatory);
  return result;
}

std::vector<correspondence::Pivot>
BuildPivots(const internal::Contour &contour,
            const std::vector<float> &positions,
            const std::vector<float> &axis_extrema,
            const std::vector<float> &curvature_extrema,
            const std::vector<float> &long_straights) {
  const std::vector<float> original = contour.MandatoryPositions(0U);
  const std::vector<float> inflections =
      contour.MandatoryPositions(kSplitInflection);
  const std::vector<float> cusps = contour.MandatoryPositions(kSplitCusps);
  const float scale = std::max(
      1.0F, std::hypot(contour.bounds().width(), contour.bounds().height()));
  std::vector<correspondence::Pivot> pivots;
  pivots.reserve(positions.size());
  for (size_t index = 0; index < positions.size(); ++index) {
    const float position = positions[index];
    const SkPoint point = PointAt(contour, position);
    constexpr float kTangentStep = 1e-4F;
    const float wrapped = position >= 1.0F ? 0.0F : position;
    const SkPoint before =
        PointAt(contour, wrapped <= kTangentStep ? 1.0F - kTangentStep
                                                 : wrapped - kTangentStep);
    const SkPoint after = PointAt(contour, wrapped >= 1.0F - kTangentStep
                                               ? kTangentStep
                                               : wrapped + kTangentStep);
    const SkVector incoming = point - before;
    const SkVector outgoing = after - point;
    const float turn =
        std::abs(std::remainder(std::atan2(outgoing.y(), outgoing.x()) -
                                    std::atan2(incoming.y(), incoming.x()),
                                2.0F * std::numbers::pi_v<float>));
    uint32_t features = correspondence::kOrdinary;
    if (ContainsPosition(original, position)) {
      features |= correspondence::kOriginalVerb;
    }
    if (turn > 0.40F) {
      features |= correspondence::kSharpCorner;
    }
    if (turn > 0.18F) {
      features |= correspondence::kTangentDiscontinuity;
    }
    if (ContainsPosition(inflections, position) &&
        !ContainsPosition(original, position)) {
      features |= correspondence::kInflection;
    }
    if (ContainsPosition(cusps, position) &&
        !ContainsPosition(original, position)) {
      features |= correspondence::kCusp;
    }
    if (ContainsPosition(axis_extrema, position)) {
      features |= correspondence::kAxisExtremum;
    }
    if (ContainsPosition(curvature_extrema, position)) {
      features |= correspondence::kCurvatureExtremum;
    }
    if (ContainsPosition(long_straights, position)) {
      features |= correspondence::kLongStraightEnd;
    }
    const SkVector radial = point - contour.center();
    pivots.push_back(
        {position, point, std::atan2(radial.y(), radial.x()),
         std::atan2(after.y() - before.y(), after.x() - before.x()),
         CurvatureAt(contour, position) * scale, features});
  }
  return pivots;
}

float MapBetweenMatches(float value,
                        const std::vector<correspondence::Match> &matches,
                        const std::vector<correspondence::Pivot> &from,
                        const std::vector<correspondence::Pivot> &to,
                        bool inverse) {
  for (size_t index = 1; index < matches.size(); ++index) {
    const float from0 = inverse
                            ? to[matches[index - 1U].targetIndex].position
                            : from[matches[index - 1U].sourceIndex].position;
    const float from1 = inverse ? to[matches[index].targetIndex].position
                                : from[matches[index].sourceIndex].position;
    if (value <= from1 + 1e-6F) {
      const float to0 = inverse ? from[matches[index - 1U].sourceIndex].position
                                : to[matches[index - 1U].targetIndex].position;
      const float to1 = inverse ? from[matches[index].sourceIndex].position
                                : to[matches[index].targetIndex].position;
      const float ratio =
          from1 > from0 ? (value - from0) / (from1 - from0) : 0.0F;
      return std::lerp(to0, to1, std::clamp(ratio, 0.0F, 1.0F));
    }
  }
  return 1.0F;
}

PreparedContour PrepareContourWithFeatureCorrespondence(
    internal::Contour source, internal::Contour target,
    const MorphOptions &options, StartPointMode start_mode,
    const std::optional<SkPoint> &source_start,
    const std::optional<SkPoint> &target_start) {
  PreparedContour prepared;
  prepared.source_original_start = source.spans().front().curve.start();
  prepared.target_original_start = target.spans().front().curve.start();
  AlignContourStarts(&source, &target, start_mode, source_start, target_start);
  prepared.source_shifted_start = source.spans().front().curve.start();
  prepared.target_shifted_start = target.spans().front().curve.start();

  std::vector<float> source_axis, source_curvature, source_straights;
  std::vector<float> target_axis, target_curvature, target_straights;
  const int half_budget = options.maximumSegmentsPerContour / 2;
  const std::vector<float> source_positions =
      BuildRevisionPositions(source, options, half_budget, &source_axis,
                             &source_curvature, &source_straights);
  const std::vector<float> target_positions = BuildRevisionPositions(
      target, options, options.maximumSegmentsPerContour - half_budget,
      &target_axis, &target_curvature, &target_straights);
  if (source_positions.empty() || target_positions.empty()) {
    return prepared;
  }
  const auto source_pivots = BuildPivots(source, source_positions, source_axis,
                                         source_curvature, source_straights);
  const auto target_pivots = BuildPivots(target, target_positions, target_axis,
                                         target_curvature, target_straights);
  const auto matches =
      correspondence::SolveMonotonic(source_pivots, target_pivots);
  if (matches.size() < 2U) {
    return prepared;
  }

  std::vector<std::pair<float, float>> knots;
  knots.reserve(source_positions.size() + target_positions.size());
  for (float position : source_positions) {
    knots.emplace_back(position,
                       MapBetweenMatches(position, matches, source_pivots,
                                         target_pivots, false));
  }
  for (float position : target_positions) {
    knots.emplace_back(MapBetweenMatches(position, matches, source_pivots,
                                         target_pivots, true),
                       position);
  }
  std::sort(knots.begin(), knots.end());
  knots.erase(
      std::unique(knots.begin(), knots.end(),
                  [](const auto &first, const auto &second) {
                    return std::abs(first.first - second.first) <= 1e-5F &&
                           std::abs(first.second - second.second) <= 1e-5F;
                  }),
      knots.end());
  for (size_t index = 1; index < knots.size(); ++index) {
    if (knots[index].first - knots[index - 1U].first <= 1e-6F ||
        knots[index].second - knots[index - 1U].second <= 1e-6F) {
      continue;
    }
    prepared.segments.push_back(
        {source.CurveBetween(knots[index - 1U].first, knots[index].first)
             .ToCanonicalCubic(),
         target.CurveBetween(knots[index - 1U].second, knots[index].second)
             .ToCanonicalCubic()});
  }
  return prepared;
}

PreparedContour
PrepareContourForOptions(internal::Contour source, internal::Contour target,
                         const MorphOptions &options, StartPointMode start_mode,
                         const std::optional<SkPoint> &source_start,
                         const std::optional<SkPoint> &target_start) {
  if (options.enableFeaturePivotCorrespondence) {
    return PrepareContourWithFeatureCorrespondence(
        std::move(source), std::move(target), options, start_mode, source_start,
        target_start);
  }
  return PrepareContour(std::move(source), std::move(target), options,
                        start_mode, source_start, target_start);
}

geometry::Cubic CollapsedCubic(SkPoint center) {
  return {{center, center, center, center}};
}

PreparedContour PrepareDisappearingContour(internal::Contour contour,
                                           bool disappears,
                                           const MorphOptions &options,
                                           SkPoint source_outer_center,
                                           SkPoint target_outer_center) {
  const SkPoint local_center =
      contour.center() -
      (disappears ? source_outer_center : target_outer_center);
  const SkPoint source_center =
      disappears ? contour.center() : source_outer_center + local_center;
  const SkPoint target_center =
      disappears ? target_outer_center + local_center : contour.center();
  PreparedContour prepared = PrepareContourForOptions(
      contour, contour, options, StartPointMode::kAutomatic, std::nullopt,
      std::nullopt);
  for (CubicPair &pair : prepared.segments) {
    if (disappears) {
      pair.target = CollapsedCubic(target_center);
    } else {
      pair.source = CollapsedCubic(source_center);
    }
  }
  if (disappears) {
    prepared.target_original_start = target_center;
    prepared.target_shifted_start = target_center;
    prepared.target_present = false;
  } else {
    prepared.source_original_start = source_center;
    prepared.source_shifted_start = source_center;
    prepared.source_present = false;
  }
  return prepared;
}

internal::CubicContour CubicsAt(const PreparedContour &contour, bool source) {
  internal::CubicContour result;
  result.reserve(contour.segments.size());
  for (const CubicPair &segment : contour.segments) {
    result.push_back(source ? segment.source : segment.target);
  }
  return result;
}

void MakeRelativeToOuter(PreparedContour *contour, SkPoint source_origin,
                         SkPoint target_origin) {
  contour->relative_to_outer = true;
  contour->source_origin = source_origin;
  contour->target_origin = target_origin;
  contour->source_original_start -= source_origin - SkPoint::Make(0.0F, 0.0F);
  contour->source_shifted_start -= source_origin - SkPoint::Make(0.0F, 0.0F);
  contour->target_original_start -= target_origin - SkPoint::Make(0.0F, 0.0F);
  contour->target_shifted_start -= target_origin - SkPoint::Make(0.0F, 0.0F);
  for (CubicPair &segment : contour->segments) {
    for (SkPoint &point : segment.source.points) {
      point -= source_origin - SkPoint::Make(0.0F, 0.0F);
    }
    for (SkPoint &point : segment.target.points) {
      point -= target_origin - SkPoint::Make(0.0F, 0.0F);
    }
  }
}

void SetRequiredClearance(PreparedContour *hole, const PreparedContour &outer,
                          float requested_clearance) {
  if (requested_clearance <= 0.0F) {
    return;
  }
  const SkPath source_outer =
      internal::MakeSimpleOuterPath(CubicsAt(outer, true));
  const SkPath target_outer =
      internal::MakeSimpleOuterPath(CubicsAt(outer, false));
  const float source_clearance =
      internal::HoleClearance(CubicsAt(*hole, true), source_outer);
  const float target_clearance =
      internal::HoleClearance(CubicsAt(*hole, false), target_outer);
  const float endpoint_clearance = std::min(source_clearance, target_clearance);
  hole->required_clearance =
      std::min(requested_clearance, endpoint_clearance * 0.9F);
}

internal::CubicContour InterpolateContour(const PreparedContour &contour,
                                          float t) {
  internal::CubicContour result;
  result.reserve(contour.segments.size());
  const SkPoint origin =
      LerpPoint(contour.source_origin, contour.target_origin, t);
  for (const CubicPair &segment : contour.segments) {
    geometry::Cubic cubic;
    for (size_t point_index = 0; point_index < cubic.points.size();
         ++point_index) {
      cubic.points[point_index] =
          LerpPoint(segment.source.points[point_index],
                    segment.target.points[point_index], t);
      if (contour.relative_to_outer) {
        cubic.points[point_index] += origin - SkPoint::Make(0.0F, 0.0F);
      }
    }
    result.push_back(cubic);
  }
  return result;
}

SkPoint InterpolateDiagnosticPoint(const PreparedContour &contour,
                                   SkPoint source, SkPoint target, float t) {
  SkPoint result = LerpPoint(source, target, t);
  if (contour.relative_to_outer) {
    result += LerpPoint(contour.source_origin, contour.target_origin, t) -
              SkPoint::Make(0.0F, 0.0F);
  }
  return result;
}

SkPoint RemapDeformedPoint(SkPoint point, const internal::CubicContour &before,
                           const internal::CubicContour &after) {
  if (before.size() != after.size() || before.empty()) {
    return point;
  }
  float best_distance_squared = std::numeric_limits<float>::infinity();
  size_t best_segment = 0;
  float best_parameter = 0.0F;
  constexpr int kSamples = 12;
  for (size_t segment = 0; segment < before.size(); ++segment) {
    const geometry::Cubic &cubic = before[segment];
    const geometry::Curve curve = geometry::Curve::CubicBezier(
        cubic.points[0], cubic.points[1], cubic.points[2], cubic.points[3]);
    for (int sample = 0; sample <= kSamples; ++sample) {
      const float parameter = static_cast<float>(sample) / kSamples;
      const SkVector delta = curve.Evaluate(parameter) - point;
      const float distance_squared =
          delta.x() * delta.x() + delta.y() * delta.y();
      if (distance_squared < best_distance_squared) {
        best_distance_squared = distance_squared;
        best_segment = segment;
        best_parameter = parameter;
      }
    }
  }
  const geometry::Cubic &mapped = after[best_segment];
  return geometry::Curve::CubicBezier(mapped.points[0], mapped.points[1],
                                      mapped.points[2], mapped.points[3])
      .Evaluate(best_parameter);
}

bool ContourIsVisible(const PreparedContour &contour, float t) {
  constexpr float kEndpointEpsilon = 1e-6F;
  if (t <= kEndpointEpsilon) {
    return contour.source_present;
  }
  if (t >= 1.0F - kEndpointEpsilon) {
    return contour.target_present;
  }
  return contour.source_present || contour.target_present;
}

std::vector<size_t> SortedHoleIndices(const internal::ParsedShape &shape) {
  const SkPoint outer_center = shape.contours[shape.outer_index].center();
  std::vector<size_t> indices;
  indices.reserve(shape.contours.size() - 1U);
  for (size_t index = 0; index < shape.contours.size(); ++index) {
    if (index != shape.outer_index) {
      indices.push_back(index);
    }
  }
  std::sort(indices.begin(), indices.end(), [&](size_t first, size_t second) {
    const SkVector first_offset = shape.contours[first].center() - outer_center;
    const SkVector second_offset =
        shape.contours[second].center() - outer_center;
    const float first_angle = std::atan2(first_offset.y(), first_offset.x());
    const float second_angle = std::atan2(second_offset.y(), second_offset.x());
    if (first_angle != second_angle) {
      return first_angle < second_angle;
    }
    const float first_radius_squared = first_offset.x() * first_offset.x() +
                                       first_offset.y() * first_offset.y();
    const float second_radius_squared = second_offset.x() * second_offset.x() +
                                        second_offset.y() * second_offset.y();
    return first_radius_squared < second_radius_squared;
  });
  return indices;
}

} // namespace

class ShapeMorpher::Impl {
public:
  bool Init(const SkPath &source_path, const SkPath &target_path,
            const MorphOptions &requested_options) {
    initialized_ = false;
    contours_.clear();
    segment_count_ = 0;
    error_.clear();
    if (!ValidateOptions(requested_options, &error_)) {
      return false;
    }
    options_ = requested_options;

    const SkMatrix source_rotation =
        RotationForPath(source_path, options_.sourceRotationDegrees);
    const SkMatrix target_rotation =
        RotationForPath(target_path, options_.targetRotationDegrees);
    const SkPath source = RotatedPath(source_path, source_rotation);
    const SkPath target = RotatedPath(target_path, target_rotation);
    if (options_.sourceStartPoint.has_value()) {
      options_.sourceStartPoint =
          MappedPoint(source_rotation, *options_.sourceStartPoint);
    }
    if (options_.targetStartPoint.has_value()) {
      options_.targetStartPoint =
          MappedPoint(target_rotation, *options_.targetStartPoint);
    }

    internal::ParsedShape source_shape;
    internal::ParsedShape target_shape;
    if (!internal::ParseClosedShape(source, &source_shape, &error_)) {
      error_ = "Source: " + error_;
      return false;
    }
    if (!internal::ParseClosedShape(target, &target_shape, &error_)) {
      error_ = "Target: " + error_;
      return false;
    }

    const internal::Contour &source_outer =
        source_shape.contours[source_shape.outer_index];
    const internal::Contour &target_outer =
        target_shape.contours[target_shape.outer_index];
    const SkPoint source_outer_center = source_outer.center();
    const SkPoint target_outer_center = target_outer.center();
    contours_.push_back(PrepareContourForOptions(
        source_outer, target_outer, options_, options_.startPointMode,
        options_.sourceStartPoint, options_.targetStartPoint));
    if (contours_.back().segments.empty()) {
      error_ = "Outer contour could not be subdivided.";
      contours_.clear();
      return false;
    }

    const std::vector<size_t> source_holes = SortedHoleIndices(source_shape);
    const std::vector<size_t> target_holes = SortedHoleIndices(target_shape);
    const size_t matched_count =
        std::min(source_holes.size(), target_holes.size());
    for (size_t index = 0; index < matched_count; ++index) {
      PreparedContour hole = PrepareContourForOptions(
          source_shape.contours[source_holes[index]],
          target_shape.contours[target_holes[index]], options_,
          options_.startPointMode == StartPointMode::kFirstVerb
              ? StartPointMode::kFirstVerb
              : StartPointMode::kAutomatic,
          std::nullopt, std::nullopt);
      SetRequiredClearance(&hole, contours_.front(),
                           options_.holeClearanceEpsilon);
      MakeRelativeToOuter(&hole, source_outer_center, target_outer_center);
      contours_.push_back(std::move(hole));
    }
    for (size_t index = matched_count; index < source_holes.size(); ++index) {
      PreparedContour hole = PrepareDisappearingContour(
          source_shape.contours[source_holes[index]], true, options_,
          source_outer_center, target_outer_center);
      SetRequiredClearance(&hole, contours_.front(),
                           options_.holeClearanceEpsilon);
      MakeRelativeToOuter(&hole, source_outer_center, target_outer_center);
      contours_.push_back(std::move(hole));
    }
    for (size_t index = matched_count; index < target_holes.size(); ++index) {
      PreparedContour hole = PrepareDisappearingContour(
          target_shape.contours[target_holes[index]], false, options_,
          source_outer_center, target_outer_center);
      SetRequiredClearance(&hole, contours_.front(),
                           options_.holeClearanceEpsilon);
      MakeRelativeToOuter(&hole, source_outer_center, target_outer_center);
      contours_.push_back(std::move(hole));
    }

    for (const PreparedContour &contour : contours_) {
      if (contour.segments.empty()) {
        error_ = "A hole contour could not be subdivided.";
        contours_.clear();
        segment_count_ = 0;
        return false;
      }
      segment_count_ += contour.segments.size();
    }
    initialized_ = true;
    return true;
  }

  SkPath GetMorphed(float t) const {
    if (!initialized_) {
      return {};
    }
    if (!std::isfinite(t)) {
      t = 0.0F;
    }
    t = std::clamp(t, 0.0F, 1.0F);

    const internal::CubicContour outer_cubics =
        InterpolateContour(contours_.front(), t);
    const SkPath outer = internal::MakeSimpleOuterPath(outer_cubics);

    SkPathBuilder builder;
    builder.setFillType(SkPathFillType::kEvenOdd);
    builder.addPath(outer);
    for (size_t index = 1; index < contours_.size(); ++index) {
      internal::CubicContour hole = InterpolateContour(contours_[index], t);
      internal::EnforceHoleClearance(&hole, outer,
                                     contours_[index].required_clearance);
      builder.addPath(internal::BuildCubicContourPath(hole));
    }
    return builder.detach();
  }

  std::vector<ContourStartPoints> GetStartPoints(float t) const {
    std::vector<ContourStartPoints> result;
    if (!initialized_) {
      return result;
    }
    if (!std::isfinite(t)) {
      t = 0.0F;
    }
    t = std::clamp(t, 0.0F, 1.0F);

    const internal::CubicContour outer_cubics =
        InterpolateContour(contours_.front(), t);
    const SkPath outer = internal::MakeSimpleOuterPath(outer_cubics);
    result.reserve(contours_.size());
    for (size_t index = 0; index < contours_.size(); ++index) {
      const PreparedContour &prepared = contours_[index];
      if (!ContourIsVisible(prepared, t)) {
        continue;
      }
      const internal::CubicContour before = InterpolateContour(prepared, t);
      internal::CubicContour displayed = before;
      if (index > 0U) {
        internal::EnforceHoleClearance(&displayed, outer,
                                       prepared.required_clearance);
      }

      SkPoint original =
          InterpolateDiagnosticPoint(prepared, prepared.source_original_start,
                                     prepared.target_original_start, t);
      SkPoint shifted =
          InterpolateDiagnosticPoint(prepared, prepared.source_shifted_start,
                                     prepared.target_shifted_start, t);
      if (index > 0U) {
        original = RemapDeformedPoint(original, before, displayed);
        shifted = RemapDeformedPoint(shifted, before, displayed);
      }

      ContourStartPoints diagnostics;
      diagnostics.original = original;
      const SkVector start_delta = shifted - original;
      if (start_delta.x() * start_delta.x() +
              start_delta.y() * start_delta.y() >
          1e-4F) {
        diagnostics.shifted = shifted;
      }
      diagnostics.isHole = index > 0U;
      result.push_back(std::move(diagnostics));
    }
    return result;
  }

  bool initialized_ = false;
  MorphOptions options_;
  std::vector<PreparedContour> contours_;
  size_t segment_count_ = 0;
  std::string error_;
};

ShapeMorpher::ShapeMorpher() : impl_(std::make_unique<Impl>()) {}

ShapeMorpher::~ShapeMorpher() = default;

ShapeMorpher::ShapeMorpher(ShapeMorpher &&) noexcept = default;

ShapeMorpher &ShapeMorpher::operator=(ShapeMorpher &&) noexcept = default;

bool ShapeMorpher::Init(const SkPath &source, const SkPath &target,
                        const MorphOptions &options) {
  return impl_->Init(source, target, options);
}

SkPath ShapeMorpher::GetMorphed(float t) const {
  return impl_ == nullptr ? SkPath{} : impl_->GetMorphed(t);
}

std::vector<ContourStartPoints> ShapeMorpher::GetStartPoints(float t) const {
  return impl_ == nullptr ? std::vector<ContourStartPoints>{}
                          : impl_->GetStartPoints(t);
}

bool ShapeMorpher::isInitialized() const {
  return impl_ != nullptr && impl_->initialized_;
}

size_t ShapeMorpher::contourCount() const {
  return impl_ == nullptr ? 0U : impl_->contours_.size();
}

size_t ShapeMorpher::segmentCount() const {
  return impl_ == nullptr ? 0U : impl_->segment_count_;
}

const std::string &ShapeMorpher::error() const {
  static const std::string kMovedFromError = "ShapeMorpher was moved from.";
  return impl_ == nullptr ? kMovedFromError : impl_->error_;
}

} // namespace skmorph
