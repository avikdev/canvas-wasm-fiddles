#pragma once

#include <vector>

#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

namespace geometry {

// Uniform scalar samples over a rectangular domain. `column_count` and
// `row_count` describe sample points, not cells; values are row-major.
struct ScalarGrid {
  SkRect bounds = SkRect::MakeEmpty();
  int column_count = 0;
  int row_count = 0;
  std::vector<float> values;

  bool IsValid() const;
  float ValueAt(int column, int row) const;
  SkPoint PointAt(int column, int row) const;
};

struct ContourPolyline {
  std::vector<SkPoint> points;
  bool closed = false;
};

// Extracts a threshold isocontour with marching squares. Ambiguous saddle
// cells use their bilinear center value as an asymptotic-decider
// approximation. Matching grid-edge endpoints are stitched into maximally
// continuous ordered open polylines or closed loops.
std::vector<ContourPolyline>
ExtractMarchingSquaresContours(const ScalarGrid &grid, float threshold);

} // namespace geometry
