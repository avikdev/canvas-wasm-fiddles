#include "geometry/catmull_rom_spline.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

int CountVerb(const SkPath &path, SkPath::Verb expected) {
  int count = 0;
  SkPath::RawIter iterator(path);
  SkPoint points[4];
  while (true) {
    const SkPath::Verb verb = iterator.next(points);
    if (verb == SkPath::kDone_Verb) {
      break;
    }
    if (verb == expected) {
      ++count;
    }
  }
  return count;
}

} // namespace

int main() {
  bool success = true;

  const std::vector<SkPoint> square = {
      {0.0F, 0.0F}, {10.0F, 0.0F}, {10.0F, 10.0F}, {0.0F, 10.0F}};
  geometry::CatmullRomOptions closed;
  closed.closed = true;
  const SkPath closed_path = geometry::CatmullRomToCubicPath(square, closed);
  success &= Expect(!closed_path.isEmpty(),
                    "A four-point closed spline should produce a path.");
  success &= Expect(CountVerb(closed_path, SkPath::kCubic_Verb) == 4,
                    "A closed spline should emit one cubic per point.");
  success &= Expect(CountVerb(closed_path, SkPath::kClose_Verb) == 1,
                    "A closed spline should close its SkPath contour.");

  const std::vector<SkPoint> line = {{0.0F, 0.0F}, {4.0F, 2.0F}, {8.0F, 0.0F}};
  const SkPath open_path = geometry::CatmullRomToCubicPath(line);
  success &= Expect(CountVerb(open_path, SkPath::kCubic_Verb) == 2,
                    "An open spline should connect adjacent points.");
  success &= Expect(CountVerb(open_path, SkPath::kClose_Verb) == 0,
                    "An open spline must not close its contour.");

  const std::vector<SkPoint> repeated = {
      {0.0F, 0.0F}, {0.0F, 0.0F}, {4.0F, 0.0F}, {4.0F, 4.0F}, {0.0F, 0.0F}};
  const SkPath deduplicated = geometry::CatmullRomToCubicPath(repeated, closed);
  success &= Expect(CountVerb(deduplicated, SkPath::kCubic_Verb) == 3,
                    "Duplicate neighbors and a repeated closing point should "
                    "not create zero-length segments.");

  geometry::CatmullRomOptions invalid_options;
  invalid_options.tension = std::numeric_limits<float>::quiet_NaN();
  success &=
      Expect(geometry::CatmullRomToCubicPath(square, invalid_options).isEmpty(),
             "Non-finite tension should be rejected.");
  const std::vector<SkPoint> invalid_points = {
      {0.0F, 0.0F}, {std::numeric_limits<float>::infinity(), 1.0F}};
  success &= Expect(geometry::CatmullRomToCubicPath(invalid_points).isEmpty(),
                    "Non-finite coordinates should be rejected.");

  return success ? 0 : 1;
}
