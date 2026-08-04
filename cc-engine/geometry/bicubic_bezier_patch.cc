#include "geometry/bicubic_bezier_patch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace geometry {
namespace {

std::array<float, 4> Bernstein(float parameter) {
  const float inverse = 1.0F - parameter;
  return {
      inverse * inverse * inverse,
      3.0F * parameter * inverse * inverse,
      3.0F * parameter * parameter * inverse,
      parameter * parameter * parameter,
  };
}

std::array<float, 4> BernsteinDerivative(float parameter) {
  const float inverse = 1.0F - parameter;
  return {
      -3.0F * inverse * inverse,
      3.0F * inverse * inverse - 6.0F * parameter * inverse,
      6.0F * parameter * inverse - 3.0F * parameter * parameter,
      3.0F * parameter * parameter,
  };
}

float SafeParameter(float parameter) {
  return std::isfinite(parameter) ? std::clamp(parameter, 0.0F, 1.0F) : 0.0F;
}

SkPoint WeightedSum(const BicubicBezierPatch::ControlPoints &points,
                    const std::array<float, 4> &u_weights,
                    const std::array<float, 4> &v_weights) {
  SkPoint result = {0.0F, 0.0F};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      const float weight = u_weights[column] * v_weights[row];
      const SkPoint &point = points[row * 4 + column];
      result.fX += point.fX * weight;
      result.fY += point.fY * weight;
    }
  }
  return result;
}

} // namespace

BicubicBezierPatch::BicubicBezierPatch(ControlPoints control_points)
    : control_points_(std::move(control_points)) {}

const BicubicBezierPatch::ControlPoints &
BicubicBezierPatch::controlPoints() const {
  return control_points_;
}

bool BicubicBezierPatch::isFinite() const {
  for (const SkPoint &point : control_points_) {
    if (!std::isfinite(point.fX) || !std::isfinite(point.fY)) {
      return false;
    }
  }
  return true;
}

SkPoint BicubicBezierPatch::Evaluate(float u, float v) const {
  if (!isFinite()) {
    return {0.0F, 0.0F};
  }
  return WeightedSum(control_points_, Bernstein(SafeParameter(u)),
                     Bernstein(SafeParameter(v)));
}

SkPoint BicubicBezierPatch::DerivativeU(float u, float v) const {
  if (!isFinite()) {
    return {0.0F, 0.0F};
  }
  return WeightedSum(control_points_, BernsteinDerivative(SafeParameter(u)),
                     Bernstein(SafeParameter(v)));
}

SkPoint BicubicBezierPatch::DerivativeV(float u, float v) const {
  if (!isFinite()) {
    return {0.0F, 0.0F};
  }
  return WeightedSum(control_points_, Bernstein(SafeParameter(u)),
                     BernsteinDerivative(SafeParameter(v)));
}

} // namespace geometry
