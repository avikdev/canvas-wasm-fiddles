#include "morphing/path_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "include/core/SkPathTypes.h"

namespace skmorph::internal {
namespace {

constexpr float kLengthTolerance = 0.005F;
constexpr float kPositionEpsilon = 1e-6F;
constexpr uint32_t kSplitInflection = 1U << 3U;
constexpr uint32_t kSplitCusps = 1U << 4U;

float Dot(SkPoint point, SkVector direction) {
  return point.x() * direction.x() + point.y() * direction.y();
}

float DistanceSquared(SkPoint first, SkPoint second) {
  const SkVector delta = first - second;
  return delta.x() * delta.x() + delta.y() * delta.y();
}

void AppendUnique(std::vector<float> *values, float value) {
  if (!std::isfinite(value)) {
    return;
  }
  value = std::clamp(value, 0.0F, 1.0F);
  for (float existing : *values) {
    if (std::abs(existing - value) <= 1e-5F) {
      return;
    }
  }
  values->push_back(value);
}

bool PointInContour(const Contour &contour, SkPoint point) {
  // Even-odd ray casting over a sufficiently dense flattening. This is only
  // used to validate the one-outer-plus-holes topology during initialization.
  bool inside = false;
  for (const Contour::Span &span : contour.spans()) {
    constexpr int kSamples = 16;
    SkPoint previous = span.curve.Evaluate(0.0F);
    for (int index = 1; index <= kSamples; ++index) {
      const SkPoint current =
          span.curve.Evaluate(static_cast<float>(index) / kSamples);
      const bool crosses =
          (previous.y() > point.y()) != (current.y() > point.y());
      if (crosses) {
        const float x = previous.x() + (point.y() - previous.y()) *
                                           (current.x() - previous.x()) /
                                           (current.y() - previous.y());
        if (x > point.x()) {
          inside = !inside;
        }
      }
      previous = current;
    }
  }
  return inside;
}

} // namespace

Contour::Contour(std::vector<geometry::Curve> curves) {
  spans_.reserve(curves.size());
  for (geometry::Curve &curve : curves) {
    if (curve.IsFinite() && !curve.IsDegenerate()) {
      spans_.push_back({std::move(curve), {}, 0.0F, 0.0F});
    }
  }
  RebuildMetrics();
}

bool Contour::valid() const {
  return !spans_.empty() && length_ > kPositionEpsilon &&
         std::abs(signed_area_) > kPositionEpsilon;
}

const std::vector<Contour::Span> &Contour::spans() const { return spans_; }

float Contour::length() const { return length_; }

float Contour::signed_area() const { return signed_area_; }

SkPoint Contour::center() const { return center_; }

SkRect Contour::bounds() const { return bounds_; }

void Contour::Reverse() {
  std::reverse(spans_.begin(), spans_.end());
  for (Span &span : spans_) {
    span.curve = span.curve.Reversed();
  }
  RebuildMetrics();
}

void Contour::RotateStart(float normalized_length) {
  if (!valid()) {
    return;
  }
  float position = normalized_length - std::floor(normalized_length);
  if (position <= kPositionEpsilon || position >= 1.0F - kPositionEpsilon) {
    return;
  }
  const Location location = Locate(position);
  const Span selected = spans_[location.span_index];
  const auto [left, right] = selected.curve.Split(location.parameter);

  std::vector<geometry::Curve> reordered;
  reordered.reserve(spans_.size() + 1U);
  if (!right.IsDegenerate()) {
    reordered.push_back(right);
  }
  for (size_t index = location.span_index + 1U; index < spans_.size();
       ++index) {
    reordered.push_back(spans_[index].curve);
  }
  for (size_t index = 0; index < location.span_index; ++index) {
    reordered.push_back(spans_[index].curve);
  }
  if (!left.IsDegenerate()) {
    reordered.push_back(left);
  }

  spans_.clear();
  spans_.reserve(reordered.size());
  for (geometry::Curve &curve : reordered) {
    spans_.push_back({std::move(curve), {}, 0.0F, 0.0F});
  }
  RebuildMetrics();
}

float Contour::ExtremePosition(SkVector direction) const {
  if (!valid()) {
    return 0.0F;
  }
  if (!direction.normalize()) {
    direction = {1.0F, 0.0F};
  }
  constexpr int kSamples = 128;
  float best_position = 0.0F;
  float best_projection = -std::numeric_limits<float>::infinity();
  for (int index = 0; index < kSamples; ++index) {
    const float position = static_cast<float>(index) / kSamples;
    const Location location = Locate(position);
    const float projection =
        Dot(spans_[location.span_index].curve.Evaluate(location.parameter),
            direction);
    if (projection > best_projection) {
      best_projection = projection;
      best_position = position;
    }
  }

  // Refine the sampled support point without assuming a particular primitive.
  float left = best_position - 1.0F / kSamples;
  float right = best_position + 1.0F / kSamples;
  const auto projection_at = [&](float position) {
    position -= std::floor(position);
    const Location location = Locate(position);
    return Dot(spans_[location.span_index].curve.Evaluate(location.parameter),
               direction);
  };
  for (int iteration = 0; iteration < 20; ++iteration) {
    const float one_third = std::lerp(left, right, 1.0F / 3.0F);
    const float two_thirds = std::lerp(left, right, 2.0F / 3.0F);
    if (projection_at(one_third) < projection_at(two_thirds)) {
      left = one_third;
    } else {
      right = two_thirds;
    }
  }
  best_position = (left + right) * 0.5F;
  best_position -= std::floor(best_position);
  return best_position;
}

float Contour::NearestPosition(SkPoint point) const {
  if (!valid()) {
    return 0.0F;
  }
  constexpr int kSamples = 128;
  float best_position = 0.0F;
  float best_distance = std::numeric_limits<float>::infinity();
  for (int index = 0; index < kSamples; ++index) {
    const float position = static_cast<float>(index) / kSamples;
    const Location location = Locate(position);
    const float distance = DistanceSquared(
        spans_[location.span_index].curve.Evaluate(location.parameter), point);
    if (distance < best_distance) {
      best_distance = distance;
      best_position = position;
    }
  }
  float left = best_position - 1.0F / kSamples;
  float right = best_position + 1.0F / kSamples;
  const auto distance_at = [&](float position) {
    position -= std::floor(position);
    const Location location = Locate(position);
    return DistanceSquared(
        spans_[location.span_index].curve.Evaluate(location.parameter), point);
  };
  for (int iteration = 0; iteration < 20; ++iteration) {
    const float one_third = std::lerp(left, right, 1.0F / 3.0F);
    const float two_thirds = std::lerp(left, right, 2.0F / 3.0F);
    if (distance_at(one_third) < distance_at(two_thirds)) {
      right = two_thirds;
    } else {
      left = one_third;
    }
  }
  best_position = (left + right) * 0.5F;
  best_position -= std::floor(best_position);
  return best_position;
}

std::vector<float>
Contour::MandatoryPositions(uint32_t subdivision_flags) const {
  std::vector<float> positions = {0.0F, 1.0F};
  if (!valid()) {
    return positions;
  }
  for (const Span &span : spans_) {
    AppendUnique(&positions, span.start_length / length_);
    AppendUnique(&positions, span.end_length / length_);
    const auto append_features = [&](const std::vector<float> &parameters) {
      for (float parameter : parameters) {
        const float local =
            span.arc_lengths.NormalizedLengthAtParameter(parameter);
        AppendUnique(&positions,
                     std::lerp(span.start_length, span.end_length, local) /
                         length_);
      }
    };
    if ((subdivision_flags & kSplitInflection) != 0U) {
      append_features(span.curve.InflectionParameters());
    }
    if ((subdivision_flags & kSplitCusps) != 0U) {
      append_features(span.curve.CuspParameters());
    }
  }
  std::sort(positions.begin(), positions.end());
  return positions;
}

geometry::Curve Contour::CurveBetween(float normalized_start,
                                      float normalized_end) const {
  normalized_start = std::clamp(normalized_start, 0.0F, 1.0F);
  normalized_end = std::clamp(normalized_end, 0.0F, 1.0F);
  if (normalized_end < normalized_start) {
    return CurveBetween(normalized_end, normalized_start).Reversed();
  }
  const float middle = (normalized_start + normalized_end) * 0.5F;
  const Location location = Locate(middle);
  const Span &span = spans_[location.span_index];
  const float span_length = span.end_length - span.start_length;
  if (span_length <= 0.0F) {
    return geometry::Curve::Line(span.curve.start(), span.curve.end());
  }
  const float start_distance = std::clamp(
      normalized_start * length_ - span.start_length, 0.0F, span_length);
  const float end_distance = std::clamp(
      normalized_end * length_ - span.start_length, 0.0F, span_length);
  const float start_parameter =
      span.arc_lengths.ParameterAtLength(start_distance);
  const float end_parameter = span.arc_lengths.ParameterAtLength(end_distance);
  return span.curve.Subcurve(start_parameter, end_parameter);
}

Contour::Location Contour::Locate(float normalized_length) const {
  if (spans_.empty() || length_ <= 0.0F) {
    return {};
  }
  const float distance = std::clamp(normalized_length, 0.0F, 1.0F) * length_;
  const auto found = std::lower_bound(
      spans_.begin(), spans_.end(), distance,
      [](const Span &span, float value) { return span.end_length < value; });
  const size_t index =
      found == spans_.end()
          ? spans_.size() - 1U
          : static_cast<size_t>(std::distance(spans_.begin(), found));
  const Span &span = spans_[index];
  return {
      index,
      span.arc_lengths.ParameterAtLength(
          std::clamp(distance - span.start_length, 0.0F,
                     span.end_length - span.start_length)),
  };
}

void Contour::RebuildMetrics() {
  length_ = 0.0F;
  bounds_ = SkRect::MakeEmpty();
  for (Span &span : spans_) {
    span.arc_lengths = span.curve.BuildArcLengthTable(kLengthTolerance);
    span.start_length = length_;
    length_ += span.arc_lengths.length();
    span.end_length = length_;
    const SkRect curve_bounds = span.curve.BoundingBox();
    if (bounds_.isEmpty()) {
      bounds_ = curve_bounds;
    } else {
      bounds_.join(curve_bounds);
    }
  }

  // Polygonal Green's-theorem integration supplies a stable signed area and
  // centroid for contour classification and hole matching.
  double twice_area = 0.0;
  double centroid_x_accumulator = 0.0;
  double centroid_y_accumulator = 0.0;
  for (const Span &span : spans_) {
    constexpr int kSamples = 16;
    SkPoint previous = span.curve.Evaluate(0.0F);
    for (int index = 1; index <= kSamples; ++index) {
      const SkPoint current =
          span.curve.Evaluate(static_cast<float>(index) / kSamples);
      const double cross = static_cast<double>(previous.x()) * current.y() -
                           static_cast<double>(current.x()) * previous.y();
      twice_area += cross;
      centroid_x_accumulator += (previous.x() + current.x()) * cross;
      centroid_y_accumulator += (previous.y() + current.y()) * cross;
      previous = current;
    }
  }
  signed_area_ = static_cast<float>(twice_area * 0.5);
  if (std::abs(twice_area) > 1e-12) {
    center_ = {
        static_cast<float>(centroid_x_accumulator / (3.0 * twice_area)),
        static_cast<float>(centroid_y_accumulator / (3.0 * twice_area)),
    };
  } else if (!bounds_.isEmpty()) {
    center_ = {bounds_.centerX(), bounds_.centerY()};
  } else {
    center_ = {0.0F, 0.0F};
  }
}

bool ParseClosedShape(const SkPath &path, ParsedShape *shape,
                      std::string *error) {
  if (shape == nullptr) {
    return false;
  }
  shape->contours.clear();
  shape->outer_index = 0;
  shape->fill_type = path.getFillType();
  shape->bounds = path.getBounds();
  if (path.isEmpty()) {
    if (error != nullptr) {
      *error = "Shape is empty.";
    }
    return false;
  }
  if (path.isInverseFillType()) {
    if (error != nullptr) {
      *error = "Inverse-filled paths are not finite morphing shapes.";
    }
    return false;
  }

  std::vector<geometry::Curve> curves;
  bool contour_open = false;
  bool contour_closed = false;
  SkPoint contour_start = {0.0F, 0.0F};
  SkPoint current = {0.0F, 0.0F};

  const auto finish_contour = [&]() -> bool {
    if (!contour_open) {
      return true;
    }
    if (!contour_closed) {
      if (error != nullptr) {
        *error = "Every morphing contour must be explicitly closed.";
      }
      return false;
    }
    Contour contour(std::move(curves));
    curves.clear();
    if (contour.valid()) {
      shape->contours.push_back(std::move(contour));
    }
    contour_open = false;
    contour_closed = false;
    return true;
  };

  SkPath::RawIter iterator(path);
  SkPoint points[4];
  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    if (verb == SkPath::kMove_Verb) {
      if (!finish_contour()) {
        return false;
      }
      contour_start = points[0];
      current = points[0];
      contour_open = true;
      continue;
    }
    if (!contour_open) {
      if (error != nullptr) {
        *error = "Path data contains a drawing verb before moveTo.";
      }
      return false;
    }
    switch (verb) {
    case SkPath::kLine_Verb:
      curves.push_back(geometry::Curve::Line(points[0], points[1]));
      current = points[1];
      break;
    case SkPath::kQuad_Verb:
      curves.push_back(
          geometry::Curve::Quadratic(points[0], points[1], points[2]));
      current = points[2];
      break;
    case SkPath::kConic_Verb: {
      std::optional<geometry::Curve> conic = geometry::Curve::Conic(
          points[0], points[1], points[2], iterator.conicWeight());
      if (!conic.has_value()) {
        if (error != nullptr) {
          *error = "Path contains an invalid conic weight.";
        }
        return false;
      }
      curves.push_back(*conic);
      current = points[2];
      break;
    }
    case SkPath::kCubic_Verb:
      curves.push_back(geometry::Curve::CubicBezier(points[0], points[1],
                                                    points[2], points[3]));
      current = points[3];
      break;
    case SkPath::kClose_Verb:
      if (current != contour_start) {
        curves.push_back(geometry::Curve::Line(current, contour_start));
      }
      current = contour_start;
      contour_closed = true;
      break;
    case SkPath::kMove_Verb:
    case SkPath::kDone_Verb:
      break;
    }
  }
  if (!finish_contour()) {
    return false;
  }
  if (shape->contours.empty()) {
    if (error != nullptr) {
      *error = "Shape has no non-degenerate closed contours.";
    }
    return false;
  }

  shape->outer_index = static_cast<size_t>(std::distance(
      shape->contours.begin(),
      std::max_element(shape->contours.begin(), shape->contours.end(),
                       [](const Contour &first, const Contour &second) {
                         return std::abs(first.signed_area()) <
                                std::abs(second.signed_area());
                       })));
  const Contour &outer = shape->contours[shape->outer_index];
  for (size_t index = 0; index < shape->contours.size(); ++index) {
    if (index == shape->outer_index) {
      continue;
    }
    if (!PointInContour(outer, shape->contours[index].center())) {
      if (error != nullptr) {
        *error =
            "Only one outer contour is supported; additional contours must "
            "be holes inside it.";
      }
      return false;
    }
  }
  return true;
}

} // namespace skmorph::internal
