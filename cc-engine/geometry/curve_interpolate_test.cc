#include "geometry/curve_interpolate.h"

#include <cmath>
#include <iostream>
#include <vector>

#include "include/core/SkPathBuilder.h"
#include "include/core/SkRect.h"

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool Near(float first, float second, float tolerance = 0.05F) {
  return std::abs(first - second) <= tolerance;
}

SkPath MakeOpenSource() {
  return SkPathBuilder()
      .moveTo(-10.0F, 10.0F)
      .quadTo(0.0F, -10.0F, 10.0F, 10.0F)
      .detach();
}

SkPath MakeOpenTarget() {
  return SkPathBuilder()
      .moveTo(-20.0F, 20.0F)
      .cubicTo(-10.0F, -20.0F, 10.0F, -20.0F, 20.0F, 20.0F)
      .detach();
}

} // namespace

int main() {
  bool success = true;

  success &= Expect(geometry::GenerateUniformSplitPoints(0).empty(),
                    "Zero splits should produce no fractions.");
  const std::vector<float> interior = geometry::GenerateUniformSplitPoints(4);
  success &= Expect(interior.size() == 3U && Near(interior[0], 0.25F) &&
                        Near(interior[2], 0.75F),
                    "Default split points should exclude both endpoints.");
  const std::vector<float> all =
      geometry::GenerateUniformSplitPoints(4, false, false);
  success &= Expect(all.size() == 5U && Near(all.front(), 0.0F) &&
                        Near(all.back(), 1.0F),
                    "Explicit endpoint inclusion should return [0, 1].");

  const SkPath guide =
      SkPathBuilder().moveTo(100.0F, 50.0F).lineTo(300.0F, 50.0F).detach();
  geometry::CurveInterpolate interpolation;
  success &= Expect(interpolation.Init(MakeOpenSource(), MakeOpenTarget(),
                                       guide, [](float) { return 1.0F; }),
                    "Compatible open paths should initialize.");
  const SkRect middle = interpolation.GetCurve(0.5F).computeTightBounds();
  success &=
      Expect(Near(middle.centerX(), 200.0F) && Near(middle.centerY(), 50.0F),
             "The interpolated box center should lie on the guide.");
  // North aligns with the eastward guide, exchanging source width and height.
  success &= Expect(Near(middle.height(), 30.0F, 0.2F),
                    "Lerped curve width should align with the guide tangent.");

  geometry::CurveInterpolate widened;
  success &= Expect(widened.Init(MakeOpenSource(), MakeOpenTarget(), guide,
                                 [](float) { return 2.0F; }),
                    "A positive width profile should initialize.");
  const SkRect wide_middle = widened.GetCurve(0.5F).computeTightBounds();
  success &= Expect(Near(wide_middle.height(), middle.height() * 2.0F, 0.3F),
                    "Width profile should scale perpendicular to local north.");

  const SkPath square =
      SkPathBuilder().addRect(SkRect::MakeLTRB(-10, -10, 10, 10)).detach();
  const SkPath diamond = SkPathBuilder()
                             .moveTo(0, -15)
                             .lineTo(15, 0)
                             .lineTo(0, 15)
                             .lineTo(-15, 0)
                             .close()
                             .detach();
  geometry::CurveInterpolate closed;
  success &=
      Expect(closed.Init(square, diamond, guide, [](float) { return 1.0F; }) &&
                 !closed.GetCurve(0.5F).isEmpty(),
             "Compatible closed paths should use closed-shape morphing.");

  geometry::CurveInterpolate mismatched;
  success &= Expect(!mismatched.Init(MakeOpenSource(), square, guide,
                                     [](float) { return 1.0F; }),
                    "Open and closed inputs should not be mixed.");
  geometry::CurveInterpolate invalid_profile;
  success &= Expect(invalid_profile.Init(MakeOpenSource(), MakeOpenTarget(),
                                         guide, [](float) { return 0.0F; }) &&
                        invalid_profile.GetCurve(0.5F).isEmpty(),
                    "A non-positive sampled width should return no curve.");

  return success ? 0 : 1;
}
