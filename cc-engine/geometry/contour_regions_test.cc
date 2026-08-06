#include "geometry/contour_regions.h"

#include <array>
#include <iostream>

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

geometry::ScalarGrid HorizontalRamp() {
  geometry::ScalarGrid grid;
  grid.column_count = 3;
  grid.row_count = 3;
  grid.bounds = SkRect::MakeWH(100.0F, 100.0F);
  grid.values = {
      0.0F, 0.5F, 1.0F, 0.0F, 0.5F, 1.0F, 0.0F, 0.5F, 1.0F,
  };
  return grid;
}

geometry::ScalarGrid FieldWithHole() {
  geometry::ScalarGrid grid;
  grid.column_count = 5;
  grid.row_count = 5;
  grid.bounds = SkRect::MakeWH(100.0F, 100.0F);
  grid.values.assign(25U, 1.0F);
  grid.values[12U] = 0.0F;
  return grid;
}

} // namespace

int main() {
  bool success = true;
  constexpr std::array<float, 2> kLevels = {0.25F, 0.75F};
  const std::optional<geometry::ContourRegionSet> regions =
      geometry::BuildInclusiveContourRegions(HorizontalRamp(), kLevels, 0.1F);
  success &= Expect(regions.has_value(),
                    "A valid scalar ramp should produce contour regions.");
  if (!regions.has_value()) {
    return 1;
  }

  success &= Expect(regions->size() == 3U,
                    "Two levels should produce three nested regions.");
  const SkPath *high = regions->InclusiveRegion(0U);
  const SkPath *middle = regions->InclusiveRegion(1U);
  const SkPath *full = regions->InclusiveRegion(2U);
  success &= Expect(high != nullptr && high->contains(90.0F, 50.0F) &&
                        !high->contains(50.0F, 50.0F),
                    "The highest inclusive region should contain only high "
                    "samples.");
  success &= Expect(middle != nullptr && middle->contains(50.0F, 50.0F) &&
                        middle->contains(90.0F, 50.0F),
                    "A lower inclusive region should retain higher samples.");
  success &= Expect(full != nullptr && full->contains(10.0F, 50.0F),
                    "The final inclusive region should cover the field.");
  success &=
      Expect(high != nullptr && CountVerb(*high, SkPath::kCubic_Verb) > 0,
             "A threshold region should use cubic spline boundaries.");
  const std::optional<SkPath> inclusive_via_mode =
      regions->Region(1U, geometry::ContourRegionMode::kInclusive);
  success &= Expect(inclusive_via_mode.has_value() &&
                        inclusive_via_mode->contains(90.0F, 50.0F),
                    "The mode-aware API should expose inclusive geometry.");

  const std::optional<SkPath> high_only = regions->ExclusiveRegion(0U);
  const std::optional<SkPath> middle_only =
      regions->Region(1U, geometry::ContourRegionMode::kExclusive);
  const std::optional<SkPath> low_only = regions->ExclusiveRegion(2U);
  success &= Expect(high_only.has_value() && high_only->contains(90.0F, 50.0F),
                    "The highest exclusive region needs no subtraction.");
  success &=
      Expect(middle_only.has_value() && middle_only->contains(50.0F, 50.0F) &&
                 !middle_only->contains(90.0F, 50.0F),
             "An exclusive region should subtract its higher neighbor.");
  success &= Expect(low_only.has_value() && low_only->contains(10.0F, 50.0F) &&
                        !low_only->contains(50.0F, 50.0F),
                    "The lowest exclusive region should contain only low "
                    "samples.");
  success &= Expect(!regions->ExclusiveRegion(3U).has_value(),
                    "Out-of-range region requests should fail safely.");

  constexpr std::array<float, 1> kHoleLevel = {0.5F};
  const std::optional<geometry::ContourRegionSet> regions_with_hole =
      geometry::BuildInclusiveContourRegions(FieldWithHole(), kHoleLevel, 0.1F);
  success &= Expect(regions_with_hole.has_value(),
                    "A scalar field with an inner low area should build.");
  if (regions_with_hole.has_value()) {
    const SkPath *donut = regions_with_hole->InclusiveRegion(0U);
    success &= Expect(donut != nullptr && donut->contains(10.0F, 10.0F) &&
                          !donut->contains(50.0F, 50.0F),
                      "Spline smoothing should retain opposite-winding inner "
                      "contours as holes.");
  }
  return success ? 0 : 1;
}
