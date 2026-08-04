#pragma once

#include <array>

#include "include/core/SkPoint.h"

namespace geometry {

// A 2D tensor-product bicubic Bézier patch backed by a row-major 4x4 control
// lattice. Row selects v and column selects u.
//
// Reference:
// https://visualizationlibrary.org/docs/2.1/html/pag_guide_bezier_surfaces.html
class BicubicBezierPatch final {
public:
  static constexpr int kControlCountPerAxis = 4;
  static constexpr int kControlPointCount = 16;
  using ControlPoints = std::array<SkPoint, kControlPointCount>;

  BicubicBezierPatch() = default;
  explicit BicubicBezierPatch(ControlPoints control_points);

  const ControlPoints &controlPoints() const;
  bool isFinite() const;

  // Evaluates the tensor-product Bernstein basis. Finite parameters are
  // clamped to [0, 1]; each non-finite parameter is replaced with zero.
  SkPoint Evaluate(float u, float v) const;

  // Evaluates first partial derivatives in normalized parameter space.
  SkPoint DerivativeU(float u, float v) const;
  SkPoint DerivativeV(float u, float v) const;

private:
  ControlPoints control_points_ = {};
};

} // namespace geometry
