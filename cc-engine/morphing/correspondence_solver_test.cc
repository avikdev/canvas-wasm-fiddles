#include "morphing/correspondence_solver.h"

#include <iostream>

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

skmorph::correspondence::Pivot Pivot(float position, float angle,
                                     uint32_t features = 0U) {
  return {position, {position, 0.0F}, angle, 0.0F, 0.0F, features};
}

} // namespace

int main() {
  using skmorph::correspondence::kAxisExtremum;
  using skmorph::correspondence::kSharpCorner;

  const std::vector<skmorph::correspondence::Pivot> source = {
      Pivot(0.0F, 0.0F), Pivot(0.25F, 0.0F, kSharpCorner), Pivot(0.50F, 3.0F),
      Pivot(0.75F, 1.0F, kAxisExtremum), Pivot(1.0F, 0.0F)};
  const std::vector<skmorph::correspondence::Pivot> target = {
      Pivot(0.0F, 0.0F), Pivot(0.20F, 3.0F), Pivot(0.42F, 0.0F, kSharpCorner),
      Pivot(0.78F, 1.0F, kAxisExtremum), Pivot(1.0F, 0.0F)};
  const auto matches = skmorph::correspondence::SolveMonotonic(source, target);

  bool success = true;
  success &= Expect(matches.size() >= 4U,
                    "Feature solver should retain compatible feature pairs.");
  success &= Expect(matches.front().sourceIndex == 0U &&
                        matches.front().targetIndex == 0U &&
                        matches.back().sourceIndex == source.size() - 1U &&
                        matches.back().targetIndex == target.size() - 1U,
                    "Closed-contour endpoints must remain immutable.");
  for (size_t index = 1; index < matches.size(); ++index) {
    success &=
        Expect(matches[index - 1U].sourceIndex < matches[index].sourceIndex &&
                   matches[index - 1U].targetIndex < matches[index].targetIndex,
               "Correspondence must preserve contour order.");
  }
  bool matched_corner = false;
  bool matched_extremum = false;
  for (const auto &match : matches) {
    matched_corner |= match.sourceIndex == 1U && match.targetIndex == 2U;
    matched_extremum |= match.sourceIndex == 3U && match.targetIndex == 3U;
  }
  success &= Expect(matched_corner && matched_extremum,
                    "Compatible semantic features should match despite arc "
                    "length drift.");
  return success ? 0 : 1;
}
