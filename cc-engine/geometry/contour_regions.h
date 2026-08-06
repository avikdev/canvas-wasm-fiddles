#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "geometry/marching_squares.h"
#include "include/core/SkPath.h"

namespace geometry {

enum class ContourRegionMode {
  kInclusive,
  kExclusive,
};

// Stores nested scalar-field regions from highest to lowest elevation. Each
// inclusive region contains the previous, higher region; the final entry is
// the complete field bounds.
class ContourRegionSet {
public:
  explicit ContourRegionSet(std::vector<SkPath> inclusive_regions);

  std::size_t size() const { return inclusive_regions_.size(); }
  const SkPath *InclusiveRegion(std::size_t index) const;
  std::optional<SkPath> ExclusiveRegion(std::size_t index) const;
  std::optional<SkPath> Region(std::size_t index, ContourRegionMode mode) const;

private:
  std::vector<SkPath> inclusive_regions_;
};

// Extracts one inclusive region for each ascending threshold, plus the full
// field as the lowest region. The returned collection is ordered from highest
// to lowest elevation so ExclusiveRegion(i) can subtract inclusive region
// i - 1 lazily. Each region is built once as a compound Catmull-Rom path so
// callers can reuse exactly the same geometry for its fill and stroke.
std::optional<ContourRegionSet>
BuildInclusiveContourRegions(const ScalarGrid &grid,
                             std::span<const float> ascending_levels,
                             float spline_tension);

} // namespace geometry
