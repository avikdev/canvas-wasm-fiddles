#pragma once

#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"

namespace geometry {

// Controls conversion of a Catmull-Rom spline to cubic Bezier segments.
struct CatmullRomOptions {
  // A closed spline joins the last point to the first and emits a closed path.
  // Closed splines require at least three distinct points.
  bool closed = false;

  // Zero produces the conventional Catmull-Rom tangent magnitude. One removes
  // all tangent overshoot, leaving cubic segments with controls at their end
  // points. Values outside [0, 1] are clamped.
  float tension = 0.0F;
};

// Converts an ordered point sequence to an SkPath made exclusively from cubic
// Bezier segments. Consecutive duplicate points are discarded. Invalid
// coordinates, insufficient points, or non-finite tension produce an empty
// path so callers never receive partially valid geometry.
//
// Open splines pass through every point and duplicate the endpoint tangents.
// Closed splines use cyclic neighbors, preserving the input's contour order.
SkPath
CatmullRomToCubicPath(const std::vector<SkPoint> &points,
                      const CatmullRomOptions &options = CatmullRomOptions{});

} // namespace geometry
