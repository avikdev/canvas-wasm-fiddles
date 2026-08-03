#include "morphing/topology_guard.h"

#include <iostream>

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

skmorph::geometry::Cubic Line(SkPoint start, SkPoint end) {
  return skmorph::geometry::Curve::Line(start, end).ToCanonicalCubic();
}

skmorph::internal::CubicContour Polygon(std::initializer_list<SkPoint> points) {
  const std::vector<SkPoint> vertices(points);
  skmorph::internal::CubicContour result;
  if (vertices.size() < 2U) {
    return result;
  }
  result.reserve(vertices.size());
  for (size_t index = 0; index < vertices.size(); ++index) {
    result.push_back(
        Line(vertices[index], vertices[(index + 1U) % vertices.size()]));
  }
  return result;
}

} // namespace

int main() {
  bool success = true;

  const skmorph::internal::CubicContour bow_tie =
      Polygon({{-3.0F, -3.0F}, {3.0F, 3.0F}, {-3.0F, 3.0F}, {3.0F, -3.0F}});
  success &= Expect(
      skmorph::internal::HasSelfIntersection(bow_tie),
      "A bow-tie outer contour should be recognized as self-intersecting.");
  const SkPath simplified = skmorph::internal::MakeSimpleOuterPath(bow_tie);
  success &= Expect(!simplified.isEmpty(),
                    "A self-intersecting contour should simplify to a fill.");
  success &=
      Expect(!skmorph::internal::PathHasSelfIntersection(simplified),
             "The Path Ops safeguard should emit non-intersecting boundaries.");

  const skmorph::internal::CubicContour outer = Polygon(
      {{-10.0F, -10.0F}, {10.0F, -10.0F}, {10.0F, 10.0F}, {-10.0F, 10.0F}});
  const SkPath outer_path = skmorph::internal::BuildCubicContourPath(outer);
  skmorph::internal::CubicContour crossing_hole =
      Polygon({{7.0F, -3.0F}, {12.0F, -3.0F}, {12.0F, 3.0F}, {7.0F, 3.0F}});
  success &= Expect(
      skmorph::internal::HoleClearance(crossing_hole, outer_path) == 0.0F,
      "A hole crossing the outer boundary should have zero clearance.");
  skmorph::internal::EnforceHoleClearance(&crossing_hole, outer_path, 1.0F);
  success &= Expect(
      skmorph::internal::HoleClearance(crossing_hole, outer_path) >= 0.99F,
      "Local deformation should restore the requested hole clearance.");

  skmorph::internal::CubicContour safe_hole =
      Polygon({{-2.0F, -2.0F}, {2.0F, -2.0F}, {2.0F, 2.0F}, {-2.0F, 2.0F}});
  const SkPath original_safe_hole =
      skmorph::internal::BuildCubicContourPath(safe_hole);
  skmorph::internal::EnforceHoleClearance(&safe_hole, outer_path, 1.0F);
  success &= Expect(
      skmorph::internal::BuildCubicContourPath(safe_hole) == original_safe_hole,
      "A hole already satisfying clearance should not be deformed.");

  return success ? 0 : 1;
}
