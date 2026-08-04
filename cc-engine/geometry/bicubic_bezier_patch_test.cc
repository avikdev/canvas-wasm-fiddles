#include "geometry/bicubic_bezier_patch.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

bool Near(float actual, float expected, float tolerance = 0.001F) {
  return std::abs(actual - expected) <= tolerance;
}

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

} // namespace

int main() {
  bool success = true;
  geometry::BicubicBezierPatch::ControlPoints planar_points;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      planar_points[row * 4 + column] = {
          static_cast<float>(column) * 10.0F,
          static_cast<float>(row) * 20.0F,
      };
    }
  }
  const geometry::BicubicBezierPatch planar(planar_points);
  const SkPoint midpoint = planar.Evaluate(0.5F, 0.5F);
  success &= Expect(Near(midpoint.fX, 15.0F) && Near(midpoint.fY, 30.0F),
                    "A regular control lattice should evaluate linearly.");
  const SkPoint corner = planar.Evaluate(1.0F, 1.0F);
  success &= Expect(Near(corner.fX, 30.0F) && Near(corner.fY, 60.0F),
                    "A patch must interpolate its corner control points.");
  const SkPoint du = planar.DerivativeU(0.31F, 0.72F);
  const SkPoint dv = planar.DerivativeV(0.31F, 0.72F);
  success &= Expect(Near(du.fX, 30.0F) && Near(du.fY, 0.0F) &&
                        Near(dv.fX, 0.0F) && Near(dv.fY, 60.0F),
                    "Planar patch derivatives should be constant.");
  success &= Expect(planar.Evaluate(-2.0F, 3.0F) == planar.Evaluate(0.0F, 1.0F),
                    "Finite parameters should clamp to the patch domain.");
  success &= Expect(planar.Evaluate(std::numeric_limits<float>::quiet_NaN(),
                                    0.0F) == planar.Evaluate(0.0F, 0.0F),
                    "Non-finite parameters should use the top-left corner.");

  planar_points[5].fX = std::numeric_limits<float>::infinity();
  const geometry::BicubicBezierPatch invalid(planar_points);
  success &= Expect(!invalid.isFinite() &&
                        invalid.Evaluate(0.5F, 0.5F) == SkPoint{0.0F, 0.0F},
                    "Invalid control lattices should evaluate safely.");
  return success ? 0 : 1;
}
