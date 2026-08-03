#include "morphing/geometry_kernel.h"

#include <cmath>
#include <iostream>
#include <numbers>

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool Near(float actual, float expected, float tolerance = 1e-3F) {
  return std::abs(actual - expected) <= tolerance;
}

bool NearPoint(SkPoint actual, SkPoint expected, float tolerance = 1e-3F) {
  return Near(actual.x(), expected.x(), tolerance) &&
         Near(actual.y(), expected.y(), tolerance);
}

} // namespace

int main() {
  using skmorph::geometry::Curve;

  bool success = true;
  const Curve line = Curve::Line({0.0F, 0.0F}, {3.0F, 0.0F});
  const skmorph::geometry::Cubic line_cubic = line.ToCanonicalCubic();
  success &= Expect(NearPoint(line_cubic.points[1], {1.0F, 0.0F}),
                    "Line-to-cubic conversion should use one-third handles.");
  success &= Expect(NearPoint(line_cubic.points[2], {2.0F, 0.0F}),
                    "Line-to-cubic conversion should use two-third handles.");
  success &= Expect(Near(line.Length(), 3.0F),
                    "Straight-line length should be exact.");

  const Curve quadratic =
      Curve::Quadratic({0.0F, 0.0F}, {1.0F, 2.0F}, {2.0F, 0.0F});
  const auto [quadratic_left, quadratic_right] = quadratic.Split(0.35F);
  success &= Expect(NearPoint(quadratic_left.end(), quadratic.Evaluate(0.35F)),
                    "Quadratic left split should end at the original curve.");
  success &=
      Expect(NearPoint(quadratic_right.start(), quadratic.Evaluate(0.35F)),
             "Quadratic right split should start at the original curve.");

  const std::optional<Curve> conic =
      Curve::Conic({1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}, std::sqrt(0.5F));
  success &= Expect(conic.has_value(), "A positive conic weight is valid.");
  if (conic.has_value()) {
    const float diagonal = std::sqrt(0.5F);
    success &=
        Expect(NearPoint(conic->Evaluate(0.5F), {diagonal, diagonal}, 2e-3F),
               "Rational conic evaluation should preserve a quarter circle.");
    const auto [left, right] = conic->Split(0.5F);
    success &=
        Expect(NearPoint(left.end(), right.start()),
               "Rational conic children should meet at the exact split point.");
  }
  success &= Expect(
      !Curve::Conic({0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 0.0F}, 0.0F).has_value(),
      "Non-positive conic weights should be rejected.");

  const std::optional<Curve> arc =
      Curve::SvgArc({1.0F, 0.0F}, {-1.0F, 0.0F}, 1.0F, 1.0F, 0.0F, false, true);
  success &= Expect(arc.has_value(), "A finite SVG endpoint arc is valid.");
  if (arc.has_value()) {
    success &= Expect(NearPoint(arc->start(), {1.0F, 0.0F}),
                      "SVG arc should preserve its start point.");
    success &= Expect(NearPoint(arc->end(), {-1.0F, 0.0F}),
                      "SVG arc should preserve its end point.");
    success &=
        Expect(Near(arc->Length(1e-4F), std::numbers::pi_v<float>, 3e-3F),
               "Unit semicircle length should approximate pi.");
  }

  const Curve inflected = Curve::CubicBezier({0.0F, 0.0F}, {1.0F, 1.0F},
                                             {2.0F, -1.0F}, {3.0F, 0.0F});
  const std::vector<float> inflections = inflected.InflectionParameters();
  success &= Expect(
      inflections.size() == 1U && Near(inflections.front(), 0.5F, 1e-4F),
      "The symmetric S cubic should expose its exact midpoint inflection.");

  const skmorph::geometry::ArcLengthTable table =
      quadratic.BuildArcLengthTable(1e-4F);
  const float parameter = table.ParameterAtNormalizedLength(0.4F);
  success &= Expect(
      Near(table.NormalizedLengthAtParameter(parameter), 0.4F, 2e-3F),
      "Arc-length table forward and inverse mappings should round-trip.");

  const Curve reversed = quadratic.Reversed();
  success &= Expect(
      NearPoint(reversed.Evaluate(0.2F), quadratic.Evaluate(0.8F), 1e-4F),
      "Curve reversal should reverse parameterization.");

  return success ? 0 : 1;
}
