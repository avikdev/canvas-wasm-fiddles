#pragma once

#include <vector>

#include "include/core/SkPath.h"
#include "morphing/geometry_kernel.h"

namespace skmorph::internal {

using CubicContour = std::vector<geometry::Cubic>;

// Reconstructs one explicitly closed cubic contour.
SkPath BuildCubicContourPath(const CubicContour &contour);

// Detects intersections between non-adjacent pieces of a flattened contour.
// The test includes self-intersections contained within one cubic.
bool HasSelfIntersection(const CubicContour &contour,
                         float maximum_flattened_piece_length = 2.0F);
bool PathHasSelfIntersection(const SkPath &path);

// Resolves a self-intersecting intermediate outer contour with Skia Path Ops.
// Simple contours are returned without Path Ops so their cubic data is kept.
SkPath MakeSimpleOuterPath(const CubicContour &contour);

// Minimum sampled distance between a hole and the outer boundary. Returns zero
// when any sampled hole point is outside the outer fill.
float HoleClearance(const CubicContour &hole, const SkPath &outer);

// Applies local control-point corrections until the sampled hole stays inside
// outer with required_clearance. If a severely invalid contour cannot be
// corrected locally, it is progressively contracted toward a safe interior
// point; the final fallback is a harmless collapsed contour.
void EnforceHoleClearance(CubicContour *hole, const SkPath &outer,
                          float required_clearance);

} // namespace skmorph::internal
