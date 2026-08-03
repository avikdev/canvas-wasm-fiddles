#include "geometry/shape_builder.h"

#include <iostream>

#include "include/core/SkPath.h"
#include "include/core/SkPathMeasure.h"

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

int ContourCount(const SkPath &path) {
  int count = 0;
  SkPathMeasure measure(path, false);
  do {
    if (measure.getLength() > 0.0F) {
      ++count;
    }
  } while (measure.nextContour());
  return count;
}

} // namespace

int main() {
  bool success = true;
  constexpr SkPoint kCenter = {20.0F, 30.0F};

  const SkPath tentacled =
      geometry::shapes::MakeTentacledBlob(kCenter, 18.0F, 5, 0.2F, 73U);
  success &= Expect(!tentacled.isEmpty() && ContourCount(tentacled) == 1,
                    "A tentacled blob should be one closed contour.");

  const SkPath cross = geometry::shapes::MakeCross(kCenter, 18.0F, 0.3F);
  const SkPath star = geometry::shapes::MakeStar(kCenter, 18.0F, 7, 0.0F);
  success &= Expect(!cross.isEmpty() && !star.isEmpty(),
                    "Cross and star builders should produce paths.");

  const SkPath polygon =
      geometry::shapes::MakePolygonWithHole(kCenter, 18.0F, 6, 0.0F);
  const SkPath semicircle =
      geometry::shapes::MakeSemicircleWithHole(kCenter, 18.0F, 0.0F);
  success &= Expect(ContourCount(polygon) == 2 &&
                        polygon.getFillType() == SkPathFillType::kEvenOdd,
                    "A holed polygon should preserve two even-odd contours.");
  success &=
      Expect(ContourCount(semicircle) == 2 &&
                 semicircle.getFillType() == SkPathFillType::kEvenOdd,
             "A holed semicircle should preserve two even-odd contours.");

  success &= Expect(geometry::shapes::MakeCross(kCenter, -1.0F, 0.0F).isEmpty(),
                    "Invalid radii should produce empty paths.");
  return success ? 0 : 1;
}
