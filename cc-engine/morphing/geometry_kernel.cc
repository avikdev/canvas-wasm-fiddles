#include "morphing/geometry_kernel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace skmorph::geometry {
namespace {

constexpr float kParameterEpsilon = 1e-6F;
constexpr float kVectorEpsilonSquared = 1e-12F;

float ClampParameter(float value) { return std::clamp(value, 0.0F, 1.0F); }

SkPoint LerpPoint(SkPoint a, SkPoint b, float t) {
  return {std::lerp(a.x(), b.x(), t), std::lerp(a.y(), b.y(), t)};
}

float VectorLength(SkVector vector) {
  return std::hypot(vector.x(), vector.y());
}

float Distance(SkPoint a, SkPoint b) {
  return std::hypot(b.x() - a.x(), b.y() - a.y());
}

float Cross(SkVector a, SkVector b) { return a.x() * b.y() - a.y() * b.x(); }

float Dot(SkVector a, SkVector b) { return a.x() * b.x() + a.y() * b.y(); }

bool IsFinitePoint(SkPoint point) {
  return std::isfinite(point.x()) && std::isfinite(point.y());
}

float PointLineDistance(SkPoint point, SkPoint start, SkPoint end) {
  const SkVector chord = end - start;
  const float chord_length = VectorLength(chord);
  if (chord_length <= kParameterEpsilon) {
    return Distance(point, start);
  }
  return std::abs(Cross(point - start, chord)) / chord_length;
}

float AngleDegrees(SkVector first, SkVector second) {
  const float denominator = VectorLength(first) * VectorLength(second);
  if (denominator <= kParameterEpsilon) {
    return 0.0F;
  }
  const float cosine =
      std::clamp(Dot(first, second) / denominator, -1.0F, 1.0F);
  return std::acos(cosine) * 180.0F / std::numbers::pi_v<float>;
}

void AppendUniqueParameter(std::vector<float> *parameters, float value) {
  if (!(value > kParameterEpsilon && value < 1.0F - kParameterEpsilon) ||
      !std::isfinite(value)) {
    return;
  }
  for (float existing : *parameters) {
    if (std::abs(existing - value) <= 1e-5F) {
      return;
    }
  }
  parameters->push_back(value);
}

std::vector<float> SolveQuadratic(float a, float b, float c) {
  std::vector<float> roots;
  if (std::abs(a) <= 1e-12F) {
    if (std::abs(b) > 1e-12F) {
      roots.push_back(-c / b);
    }
    return roots;
  }
  float discriminant = b * b - 4.0F * a * c;
  if (discriminant < -1e-10F) {
    return roots;
  }
  discriminant = std::max(0.0F, discriminant);
  const float square_root = std::sqrt(discriminant);
  const float q = -0.5F * (b + std::copysign(square_root, b));
  if (std::abs(q) <= 1e-12F) {
    roots.push_back(-b / (2.0F * a));
    return roots;
  }
  roots.push_back(q / a);
  roots.push_back(c / q);
  return roots;
}

float SignedAngle(SkVector first, SkVector second) {
  return std::atan2(Cross(first, second), Dot(first, second));
}

float VectorLengthSquared(SkVector vector) {
  return vector.x() * vector.x() + vector.y() * vector.y();
}

} // namespace

float ArcLengthTable::length() const {
  return samples_.empty() ? 0.0F : samples_.back().cumulative_length;
}

bool ArcLengthTable::empty() const { return samples_.size() < 2U; }

float ArcLengthTable::ParameterAtLength(float distance) const {
  if (empty() || length() <= 0.0F) {
    return 0.0F;
  }
  const float pinned = std::clamp(distance, 0.0F, length());
  const auto upper = std::lower_bound(samples_.begin(), samples_.end(), pinned,
                                      [](const Sample &sample, float value) {
                                        return sample.cumulative_length < value;
                                      });
  if (upper == samples_.begin()) {
    return upper->parameter;
  }
  if (upper == samples_.end()) {
    return samples_.back().parameter;
  }
  const Sample &right = *upper;
  const Sample &left = *(upper - 1);
  const float span = right.cumulative_length - left.cumulative_length;
  const float ratio =
      span <= 0.0F ? 0.0F : (pinned - left.cumulative_length) / span;
  return std::lerp(left.parameter, right.parameter, ratio);
}

float ArcLengthTable::ParameterAtNormalizedLength(
    float normalized_length) const {
  return ParameterAtLength(ClampParameter(normalized_length) * length());
}

float ArcLengthTable::NormalizedLengthAtParameter(float parameter) const {
  if (empty() || length() <= 0.0F) {
    return 0.0F;
  }
  const float pinned = ClampParameter(parameter);
  const auto upper = std::lower_bound(samples_.begin(), samples_.end(), pinned,
                                      [](const Sample &sample, float value) {
                                        return sample.parameter < value;
                                      });
  if (upper == samples_.begin()) {
    return upper->cumulative_length / length();
  }
  if (upper == samples_.end()) {
    return 1.0F;
  }
  const Sample &right = *upper;
  const Sample &left = *(upper - 1);
  const float span = right.parameter - left.parameter;
  const float ratio = span <= 0.0F ? 0.0F : (pinned - left.parameter) / span;
  return std::lerp(left.cumulative_length, right.cumulative_length, ratio) /
         length();
}

const std::vector<ArcLengthTable::Sample> &ArcLengthTable::samples() const {
  return samples_;
}

Curve::Curve(Type type) : type_(type) {}

Curve Curve::Line(SkPoint start, SkPoint end) {
  Curve result(Type::kLine);
  result.points_[0] = start;
  result.points_[1] = end;
  return result;
}

Curve Curve::Quadratic(SkPoint start, SkPoint control, SkPoint end) {
  Curve result(Type::kQuadratic);
  result.points_[0] = start;
  result.points_[1] = control;
  result.points_[2] = end;
  return result;
}

Curve Curve::CubicBezier(SkPoint start, SkPoint control1, SkPoint control2,
                         SkPoint end) {
  Curve result(Type::kCubic);
  result.points_[0] = start;
  result.points_[1] = control1;
  result.points_[2] = control2;
  result.points_[3] = end;
  return result;
}

std::optional<Curve> Curve::Conic(SkPoint start, SkPoint control, SkPoint end,
                                  float weight) {
  if (!std::isfinite(weight) || weight <= 0.0F) {
    return std::nullopt;
  }
  Curve result(Type::kConic);
  result.points_[0] = start;
  result.points_[1] = control;
  result.points_[2] = end;
  result.conic_weight_ = weight;
  return result;
}

std::optional<Curve> Curve::SvgArc(SkPoint start, SkPoint end, float radius_x,
                                   float radius_y,
                                   float x_axis_rotation_degrees,
                                   bool large_arc, bool sweep) {
  if (!IsFinitePoint(start) || !IsFinitePoint(end) ||
      !std::isfinite(radius_x) || !std::isfinite(radius_y) ||
      !std::isfinite(x_axis_rotation_degrees) || start == end) {
    return std::nullopt;
  }
  radius_x = std::abs(radius_x);
  radius_y = std::abs(radius_y);
  if (radius_x <= 0.0F || radius_y <= 0.0F) {
    return std::nullopt;
  }

  const float phi = std::fmod(x_axis_rotation_degrees, 360.0F) *
                    std::numbers::pi_v<float> / 180.0F;
  const float cosine = std::cos(phi);
  const float sine = std::sin(phi);
  const float delta_x = (start.x() - end.x()) * 0.5F;
  const float delta_y = (start.y() - end.y()) * 0.5F;
  const float x_prime = cosine * delta_x + sine * delta_y;
  const float y_prime = -sine * delta_x + cosine * delta_y;

  float radii_scale = x_prime * x_prime / (radius_x * radius_x) +
                      y_prime * y_prime / (radius_y * radius_y);
  if (radii_scale > 1.0F) {
    radii_scale = std::sqrt(radii_scale);
    radius_x *= radii_scale;
    radius_y *= radii_scale;
  }

  const float rx2 = radius_x * radius_x;
  const float ry2 = radius_y * radius_y;
  const float xp2 = x_prime * x_prime;
  const float yp2 = y_prime * y_prime;
  const float numerator = std::max(0.0F, rx2 * ry2 - rx2 * yp2 - ry2 * xp2);
  const float denominator = rx2 * yp2 + ry2 * xp2;
  float center_scale =
      denominator <= 0.0F ? 0.0F : std::sqrt(numerator / denominator);
  if (large_arc == sweep) {
    center_scale = -center_scale;
  }
  const float center_x_prime = center_scale * radius_x * y_prime / radius_y;
  const float center_y_prime = -center_scale * radius_y * x_prime / radius_x;
  const SkPoint center = {
      cosine * center_x_prime - sine * center_y_prime +
          (start.x() + end.x()) * 0.5F,
      sine * center_x_prime + cosine * center_y_prime +
          (start.y() + end.y()) * 0.5F,
  };

  const SkVector start_vector = {
      (x_prime - center_x_prime) / radius_x,
      (y_prime - center_y_prime) / radius_y,
  };
  const SkVector end_vector = {
      (-x_prime - center_x_prime) / radius_x,
      (-y_prime - center_y_prime) / radius_y,
  };
  float start_angle = std::atan2(start_vector.y(), start_vector.x());
  float sweep_angle = SignedAngle(start_vector, end_vector);
  if (!sweep && sweep_angle > 0.0F) {
    sweep_angle -= 2.0F * std::numbers::pi_v<float>;
  } else if (sweep && sweep_angle < 0.0F) {
    sweep_angle += 2.0F * std::numbers::pi_v<float>;
  }

  Curve result(Type::kSvgArc);
  result.points_[0] = start;
  result.points_[1] = end;
  result.arc_ = {center, radius_x, radius_y, phi, start_angle, sweep_angle};
  return result;
}

Curve::Type Curve::type() const { return type_; }

SkPoint Curve::start() const { return points_[0]; }

SkPoint Curve::end() const {
  switch (type_) {
  case Type::kLine:
  case Type::kSvgArc:
    return points_[1];
  case Type::kQuadratic:
  case Type::kConic:
    return points_[2];
  case Type::kCubic:
    return points_[3];
  }
  return points_[0];
}

bool Curve::IsFinite() const {
  if (!IsFinitePoint(start()) || !IsFinitePoint(end())) {
    return false;
  }
  switch (type_) {
  case Type::kLine:
    return true;
  case Type::kQuadratic:
    return IsFinitePoint(points_[1]);
  case Type::kCubic:
    return IsFinitePoint(points_[1]) && IsFinitePoint(points_[2]);
  case Type::kConic:
    return IsFinitePoint(points_[1]) && std::isfinite(conic_weight_) &&
           conic_weight_ > 0.0F;
  case Type::kSvgArc:
    return IsFinitePoint(arc_.center) && std::isfinite(arc_.radius_x) &&
           std::isfinite(arc_.radius_y) &&
           std::isfinite(arc_.rotation_radians) &&
           std::isfinite(arc_.start_angle) && std::isfinite(arc_.sweep_angle);
  }
  return false;
}

bool Curve::IsDegenerate(float epsilon) const {
  const float scale =
      std::max({1.0F, std::abs(start().x()), std::abs(start().y()),
                std::abs(end().x()), std::abs(end().y())});
  return VectorLength(end() - start()) <= std::max(0.0F, epsilon) * scale &&
         Flatness() <= std::max(0.0F, epsilon) * scale;
}

SkPoint Curve::Evaluate(float parameter) const {
  const float t = ClampParameter(parameter);
  const float u = 1.0F - t;
  switch (type_) {
  case Type::kLine:
    return LerpPoint(points_[0], points_[1], t);
  case Type::kQuadratic:
    return {
        u * u * points_[0].x() + 2.0F * u * t * points_[1].x() +
            t * t * points_[2].x(),
        u * u * points_[0].y() + 2.0F * u * t * points_[1].y() +
            t * t * points_[2].y(),
    };
  case Type::kCubic:
    return {
        u * u * u * points_[0].x() + 3.0F * u * u * t * points_[1].x() +
            3.0F * u * t * t * points_[2].x() + t * t * t * points_[3].x(),
        u * u * u * points_[0].y() + 3.0F * u * u * t * points_[1].y() +
            3.0F * u * t * t * points_[2].y() + t * t * t * points_[3].y(),
    };
  case Type::kConic: {
    const float denominator = u * u + 2.0F * conic_weight_ * u * t + t * t;
    if (std::abs(denominator) <= kParameterEpsilon) {
      return points_[0];
    }
    return {
        (u * u * points_[0].x() +
         2.0F * conic_weight_ * u * t * points_[1].x() +
         t * t * points_[2].x()) /
            denominator,
        (u * u * points_[0].y() +
         2.0F * conic_weight_ * u * t * points_[1].y() +
         t * t * points_[2].y()) /
            denominator,
    };
  }
  case Type::kSvgArc: {
    const float angle = arc_.start_angle + arc_.sweep_angle * t;
    const float x = arc_.radius_x * std::cos(angle);
    const float y = arc_.radius_y * std::sin(angle);
    const float cosine = std::cos(arc_.rotation_radians);
    const float sine = std::sin(arc_.rotation_radians);
    return {arc_.center.x() + cosine * x - sine * y,
            arc_.center.y() + sine * x + cosine * y};
  }
  }
  return points_[0];
}

SkVector Curve::Derivative(float parameter) const {
  const float t = ClampParameter(parameter);
  const float u = 1.0F - t;
  switch (type_) {
  case Type::kLine:
    return points_[1] - points_[0];
  case Type::kQuadratic:
    return ((points_[1] - points_[0]) * u + (points_[2] - points_[1]) * t) *
           2.0F;
  case Type::kCubic:
    return ((points_[1] - points_[0]) * (u * u) +
            (points_[2] - points_[1]) * (2.0F * u * t) +
            (points_[3] - points_[2]) * (t * t)) *
           3.0F;
  case Type::kConic: {
    // Differentiate the rational quadratic N(t) / D(t).
    const SkVector p0 = points_[0] - SkPoint::Make(0.0F, 0.0F);
    const SkVector p1 = points_[1] - SkPoint::Make(0.0F, 0.0F);
    const SkVector p2 = points_[2] - SkPoint::Make(0.0F, 0.0F);
    const SkVector numerator =
        p0 * (u * u) + p1 * (2.0F * conic_weight_ * u * t) + p2 * (t * t);
    const SkVector numerator_derivative =
        ((p1 * conic_weight_ - p0) * u + (p2 - p1 * conic_weight_) * t) * 2.0F;
    const float denominator = u * u + 2.0F * conic_weight_ * u * t + t * t;
    const float denominator_derivative =
        2.0F * ((conic_weight_ - 1.0F) * u + (1.0F - conic_weight_) * t);
    if (std::abs(denominator) <= kParameterEpsilon) {
      return {0.0F, 0.0F};
    }
    return (numerator_derivative * denominator -
            numerator * denominator_derivative) *
           (1.0F / (denominator * denominator));
  }
  case Type::kSvgArc: {
    const float angle = arc_.start_angle + arc_.sweep_angle * t;
    const float dx = -arc_.radius_x * std::sin(angle) * arc_.sweep_angle;
    const float dy = arc_.radius_y * std::cos(angle) * arc_.sweep_angle;
    const float cosine = std::cos(arc_.rotation_radians);
    const float sine = std::sin(arc_.rotation_radians);
    return {cosine * dx - sine * dy, sine * dx + cosine * dy};
  }
  }
  return {0.0F, 0.0F};
}

SkVector Curve::SecondDerivative(float parameter) const {
  const float t = ClampParameter(parameter);
  switch (type_) {
  case Type::kLine:
    return {0.0F, 0.0F};
  case Type::kQuadratic:
    return ((points_[2] - points_[1]) - (points_[1] - points_[0])) * 2.0F;
  case Type::kCubic:
    return (((points_[2] - points_[1]) - (points_[1] - points_[0])) *
                (1.0F - t) +
            ((points_[3] - points_[2]) - (points_[2] - points_[1])) * t) *
           6.0F;
  case Type::kConic: {
    // A centered finite difference is stable for rational conics and only
    // used by curvature/subdivision tests, never endpoint construction.
    const float step = 1e-3F;
    const float left = std::max(0.0F, t - step);
    const float right = std::min(1.0F, t + step);
    const float span = right - left;
    if (span <= 0.0F) {
      return {0.0F, 0.0F};
    }
    return (Derivative(right) - Derivative(left)) * (1.0F / span);
  }
  case Type::kSvgArc: {
    const float angle = arc_.start_angle + arc_.sweep_angle * t;
    const float sweep2 = arc_.sweep_angle * arc_.sweep_angle;
    const float dx = -arc_.radius_x * std::cos(angle) * sweep2;
    const float dy = -arc_.radius_y * std::sin(angle) * sweep2;
    const float cosine = std::cos(arc_.rotation_radians);
    const float sine = std::sin(arc_.rotation_radians);
    return {cosine * dx - sine * dy, sine * dx + cosine * dy};
  }
  }
  return {0.0F, 0.0F};
}

SkVector Curve::Tangent(float parameter) const {
  SkVector tangent = Derivative(parameter);
  if (tangent.normalize()) {
    return tangent;
  }
  constexpr float kProbe = 1e-3F;
  const float t = ClampParameter(parameter);
  const SkPoint before = Evaluate(std::max(0.0F, t - kProbe));
  const SkPoint after = Evaluate(std::min(1.0F, t + kProbe));
  tangent = after - before;
  if (!tangent.normalize()) {
    return {1.0F, 0.0F};
  }
  return tangent;
}

float Curve::Curvature(float parameter) const {
  const SkVector first = Derivative(parameter);
  const SkVector second = SecondDerivative(parameter);
  const float speed_squared = VectorLengthSquared(first);
  if (speed_squared <= kVectorEpsilonSquared) {
    return 0.0F;
  }
  return Cross(first, second) / (speed_squared * std::sqrt(speed_squared));
}

std::pair<Curve, Curve> Curve::Split(float parameter) const {
  const float t = ClampParameter(parameter);
  switch (type_) {
  case Type::kLine: {
    const SkPoint middle = LerpPoint(points_[0], points_[1], t);
    return {Line(points_[0], middle), Line(middle, points_[1])};
  }
  case Type::kQuadratic: {
    const SkPoint p01 = LerpPoint(points_[0], points_[1], t);
    const SkPoint p12 = LerpPoint(points_[1], points_[2], t);
    const SkPoint middle = LerpPoint(p01, p12, t);
    return {Quadratic(points_[0], p01, middle),
            Quadratic(middle, p12, points_[2])};
  }
  case Type::kCubic: {
    const SkPoint p01 = LerpPoint(points_[0], points_[1], t);
    const SkPoint p12 = LerpPoint(points_[1], points_[2], t);
    const SkPoint p23 = LerpPoint(points_[2], points_[3], t);
    const SkPoint p012 = LerpPoint(p01, p12, t);
    const SkPoint p123 = LerpPoint(p12, p23, t);
    const SkPoint middle = LerpPoint(p012, p123, t);
    return {CubicBezier(points_[0], p01, p012, middle),
            CubicBezier(middle, p123, p23, points_[3])};
  }
  case Type::kConic: {
    // Preserve the exact rational curve by splitting homogeneous control
    // points and renormalizing each child so endpoint weights remain one.
    struct HomogeneousPoint {
      float x;
      float y;
      float w;
    };
    const auto mix = [t](HomogeneousPoint a, HomogeneousPoint b) {
      return HomogeneousPoint{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t),
                              std::lerp(a.w, b.w, t)};
    };
    const auto project = [](HomogeneousPoint point) {
      return SkPoint::Make(point.x / point.w, point.y / point.w);
    };
    const HomogeneousPoint h0 = {points_[0].x(), points_[0].y(), 1.0F};
    const HomogeneousPoint h1 = {
        points_[1].x() * conic_weight_,
        points_[1].y() * conic_weight_,
        conic_weight_,
    };
    const HomogeneousPoint h2 = {points_[2].x(), points_[2].y(), 1.0F};
    const HomogeneousPoint h01 = mix(h0, h1);
    const HomogeneousPoint h12 = mix(h1, h2);
    const HomogeneousPoint hm = mix(h01, h12);
    const float left_weight =
        h01.w / std::sqrt(std::max(kParameterEpsilon, h0.w * hm.w));
    const float right_weight =
        h12.w / std::sqrt(std::max(kParameterEpsilon, hm.w * h2.w));
    return {*Conic(project(h0), project(h01), project(hm), left_weight),
            *Conic(project(hm), project(h12), project(h2), right_weight)};
  }
  case Type::kSvgArc: {
    const float left_sweep = arc_.sweep_angle * t;
    Curve left(Type::kSvgArc);
    left.arc_ = arc_;
    left.arc_.sweep_angle = left_sweep;
    left.points_[0] = start();
    left.points_[1] = Evaluate(t);
    Curve right(Type::kSvgArc);
    right.arc_ = arc_;
    right.arc_.start_angle += left_sweep;
    right.arc_.sweep_angle -= left_sweep;
    right.points_[0] = left.points_[1];
    right.points_[1] = end();
    return {left, right};
  }
  }
  return {*this, *this};
}

Curve Curve::Subcurve(float start_parameter, float end_parameter) const {
  float start_t = ClampParameter(start_parameter);
  float end_t = ClampParameter(end_parameter);
  if (start_t > end_t) {
    return Subcurve(end_t, start_t).Reversed();
  }
  if (start_t <= 0.0F && end_t >= 1.0F) {
    return *this;
  }
  if (end_t <= start_t + kParameterEpsilon) {
    const SkPoint point = Evaluate(start_t);
    return Line(point, point);
  }
  const auto [prefix, ignored_right] = Split(end_t);
  (void)ignored_right;
  if (start_t <= 0.0F) {
    return prefix;
  }
  const auto [ignored_left, middle] = prefix.Split(start_t / end_t);
  (void)ignored_left;
  return middle;
}

Curve Curve::Reversed() const {
  switch (type_) {
  case Type::kLine:
    return Line(points_[1], points_[0]);
  case Type::kQuadratic:
    return Quadratic(points_[2], points_[1], points_[0]);
  case Type::kCubic:
    return CubicBezier(points_[3], points_[2], points_[1], points_[0]);
  case Type::kConic:
    return *Conic(points_[2], points_[1], points_[0], conic_weight_);
  case Type::kSvgArc: {
    Curve reversed(Type::kSvgArc);
    reversed.points_[0] = end();
    reversed.points_[1] = start();
    reversed.arc_ = arc_;
    reversed.arc_.start_angle += arc_.sweep_angle;
    reversed.arc_.sweep_angle = -arc_.sweep_angle;
    return reversed;
  }
  }
  return *this;
}

Cubic Curve::ToCanonicalCubic() const {
  Cubic result;
  result.points[0] = start();
  result.points[3] = end();
  switch (type_) {
  case Type::kLine: {
    const SkVector delta = end() - start();
    result.points[1] = start() + delta * (1.0F / 3.0F);
    result.points[2] = start() + delta * (2.0F / 3.0F);
    break;
  }
  case Type::kQuadratic:
    result.points[1] = points_[0] + (points_[1] - points_[0]) * (2.0F / 3.0F);
    result.points[2] = points_[2] + (points_[1] - points_[2]) * (2.0F / 3.0F);
    break;
  case Type::kCubic:
    result.points = points_;
    break;
  case Type::kConic:
  case Type::kSvgArc:
    // Endpoint-Hermite conversion is exact in position and tangent. Adaptive
    // subdivision bounds its positional approximation error.
    result.points[1] = start() + Derivative(0.0F) * (1.0F / 3.0F);
    result.points[2] = end() - Derivative(1.0F) * (1.0F / 3.0F);
    break;
  }
  return result;
}

SkRect Curve::BoundingBox() const {
  constexpr int kSamples = 32;
  const SkPoint first = Evaluate(0.0F);
  float minimum_x = first.x();
  float minimum_y = first.y();
  float maximum_x = first.x();
  float maximum_y = first.y();
  for (int index = 0; index <= kSamples; ++index) {
    const SkPoint point =
        Evaluate(static_cast<float>(index) / static_cast<float>(kSamples));
    minimum_x = std::min(minimum_x, point.x());
    minimum_y = std::min(minimum_y, point.y());
    maximum_x = std::max(maximum_x, point.x());
    maximum_y = std::max(maximum_y, point.y());
  }
  return SkRect::MakeLTRB(minimum_x, minimum_y, maximum_x, maximum_y);
}

ArcLengthTable Curve::BuildArcLengthTable(float tolerance,
                                          int max_depth) const {
  ArcLengthTable table;
  table.samples_.push_back({0.0F, 0.0F});
  if (!IsFinite()) {
    return table;
  }
  tolerance = std::max(tolerance, 1e-6F);
  max_depth = std::clamp(max_depth, 1, 24);

  struct Node {
    float start_t;
    float end_t;
    SkPoint start_point;
    SkPoint end_point;
    int depth;
  };
  std::vector<Node> stack;
  stack.push_back({0.0F, 1.0F, start(), end(), 0});
  float cumulative = 0.0F;
  while (!stack.empty()) {
    const Node node = stack.back();
    stack.pop_back();
    const float middle_t = (node.start_t + node.end_t) * 0.5F;
    const SkPoint middle = Evaluate(middle_t);
    const float chord = Distance(node.start_point, node.end_point);
    const float polygon =
        Distance(node.start_point, middle) + Distance(middle, node.end_point);
    if (node.depth >= max_depth || polygon - chord <= tolerance) {
      // The two-chord estimate is substantially more accurate than accepting
      // the endpoint chord and remains monotonic for inverse lookup.
      cumulative += polygon;
      table.samples_.push_back({node.end_t, cumulative});
      continue;
    }
    stack.push_back(
        {middle_t, node.end_t, middle, node.end_point, node.depth + 1});
    stack.push_back(
        {node.start_t, middle_t, node.start_point, middle, node.depth + 1});
  }
  if (table.samples_.size() == 1U) {
    table.samples_.push_back({1.0F, 0.0F});
  }
  return table;
}

float Curve::Length(float tolerance, int max_depth) const {
  return BuildArcLengthTable(tolerance, max_depth).length();
}

std::vector<float> Curve::InflectionParameters() const {
  std::vector<float> result;
  if (type_ != Type::kCubic) {
    return result;
  }
  const SkVector a =
      -points_[0] + points_[1] * 3.0F - points_[2] * 3.0F + points_[3];
  const SkVector b = points_[0] * 3.0F - points_[1] * 6.0F + points_[2] * 3.0F;
  const SkVector c = points_[0] * -3.0F + points_[1] * 3.0F;
  // cross(P'(t), P''(t)) = A*t^2 + B*t + C.
  const float quadratic = -3.0F * Cross(a, b);
  const float linear = 3.0F * Cross(c, a);
  const float constant = Cross(c, b);
  for (float root : SolveQuadratic(quadratic, linear, constant)) {
    AppendUniqueParameter(&result, root);
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<float> Curve::CuspParameters(float derivative_epsilon) const {
  std::vector<float> candidates;
  if (type_ == Type::kQuadratic || type_ == Type::kCubic) {
    const auto add_derivative_roots = [&](float p0, float p1, float p2,
                                          float p3, bool cubic) {
      if (cubic) {
        for (float root :
             SolveQuadratic(-p0 + 3.0F * p1 - 3.0F * p2 + p3,
                            2.0F * (p0 - 2.0F * p1 + p2), p1 - p0)) {
          AppendUniqueParameter(&candidates, root);
        }
      } else {
        const float denominator = p0 - 2.0F * p1 + p2;
        if (std::abs(denominator) > 1e-12F) {
          AppendUniqueParameter(&candidates, (p0 - p1) / denominator);
        }
      }
    };
    add_derivative_roots(points_[0].x(), points_[1].x(), points_[2].x(),
                         points_[3].x(), type_ == Type::kCubic);
    add_derivative_roots(points_[0].y(), points_[1].y(), points_[2].y(),
                         points_[3].y(), type_ == Type::kCubic);
  }

  // Rational conics and near-stationary cubic cases are covered by a small
  // deterministic scan followed by ternary refinement of speed.
  constexpr int kSamples = 32;
  for (int index = 1; index < kSamples; ++index) {
    const float t = static_cast<float>(index) / kSamples;
    const float previous_speed =
        VectorLengthSquared(Derivative(t - 1.0F / kSamples));
    const float speed = VectorLengthSquared(Derivative(t));
    const float next_speed =
        VectorLengthSquared(Derivative(t + 1.0F / kSamples));
    if (speed <= previous_speed && speed <= next_speed) {
      float left = t - 1.0F / kSamples;
      float right = t + 1.0F / kSamples;
      for (int iteration = 0; iteration < 16; ++iteration) {
        const float one_third = std::lerp(left, right, 1.0F / 3.0F);
        const float two_thirds = std::lerp(left, right, 2.0F / 3.0F);
        if (VectorLengthSquared(Derivative(one_third)) <
            VectorLengthSquared(Derivative(two_thirds))) {
          right = two_thirds;
        } else {
          left = one_third;
        }
      }
      AppendUniqueParameter(&candidates, (left + right) * 0.5F);
    }
  }

  std::vector<float> result;
  const float scale = std::max(1.0F, Length());
  const float threshold_squared =
      derivative_epsilon * derivative_epsilon * scale * scale;
  for (float candidate : candidates) {
    if (VectorLengthSquared(Derivative(candidate)) <= threshold_squared) {
      AppendUniqueParameter(&result, candidate);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

float Curve::Flatness() const {
  switch (type_) {
  case Type::kLine:
    return 0.0F;
  case Type::kQuadratic:
  case Type::kConic:
    return PointLineDistance(points_[1], start(), end());
  case Type::kCubic:
    return std::max(PointLineDistance(points_[1], start(), end()),
                    PointLineDistance(points_[2], start(), end()));
  case Type::kSvgArc:
    return std::max(PointLineDistance(Evaluate(1.0F / 3.0F), start(), end()),
                    PointLineDistance(Evaluate(2.0F / 3.0F), start(), end()));
  }
  return 0.0F;
}

float Curve::EndpointTangentAngleDegrees() const {
  return AngleDegrees(Tangent(0.0F), Tangent(1.0F));
}

float Curve::CurvatureVariation() const {
  constexpr std::array<float, 5> kSamples = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F};
  float minimum = std::numeric_limits<float>::infinity();
  float maximum = -std::numeric_limits<float>::infinity();
  for (float parameter : kSamples) {
    const float curvature = Curvature(parameter);
    if (!std::isfinite(curvature)) {
      continue;
    }
    minimum = std::min(minimum, curvature);
    maximum = std::max(maximum, curvature);
  }
  if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
    return 0.0F;
  }
  return maximum - minimum;
}

float Curve::ArcLengthError(float tolerance) const {
  const Cubic cubic = ToCanonicalCubic();
  const float chord = Distance(start(), end());
  const float control_polygon = Distance(cubic.points[0], cubic.points[1]) +
                                Distance(cubic.points[1], cubic.points[2]) +
                                Distance(cubic.points[2], cubic.points[3]);
  const float numerical = Length(tolerance);
  return std::max(std::abs(numerical - chord),
                  std::abs(control_polygon - numerical));
}

float Curve::ControlPolygonTurningAngleDegrees() const {
  const Cubic cubic = ToCanonicalCubic();
  std::array<SkVector, 3> edges = {
      cubic.points[1] - cubic.points[0],
      cubic.points[2] - cubic.points[1],
      cubic.points[3] - cubic.points[2],
  };
  float turning = 0.0F;
  SkVector previous = {0.0F, 0.0F};
  bool has_previous = false;
  for (SkVector edge : edges) {
    if (VectorLengthSquared(edge) <= kVectorEpsilonSquared) {
      continue;
    }
    if (has_previous) {
      turning += std::abs(SignedAngle(previous, edge));
    }
    previous = edge;
    has_previous = true;
  }
  return turning * 180.0F / std::numbers::pi_v<float>;
}

} // namespace skmorph::geometry
