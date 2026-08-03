#pragma once

#include <cstdint>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"

namespace geometry::shapes {

// Creates a smooth closed blob whose arms radiate from a compact body.
// outer_radius bounds the longest tentacle; tentacle_count is clamped to
// [3, 12]. The seed deterministically varies arm length, width, and bend.
SkPath MakeTentacledBlob(SkPoint center, float outer_radius, int tentacle_count,
                         float rotation_radians, std::uint32_t seed);

// Creates a symmetric four-arm cross contained by outer_radius.
SkPath MakeCross(SkPoint center, float outer_radius, float rotation_radians,
                 float arm_width_ratio = 0.30F);

// Creates two concentric polygon contours with an even-odd hole.
SkPath MakePolygonWithHole(SkPoint center, float outer_radius, int side_count,
                           float rotation_radians,
                           float hole_radius_ratio = 0.43F);

// Creates a semicircular band using a smaller semicircle as an even-odd hole.
SkPath MakeSemicircleWithHole(SkPoint center, float outer_radius,
                              float rotation_radians,
                              float hole_radius_ratio = 0.48F);

// Creates a broad-tipped star. point_count is clamped to [3, 16].
SkPath MakeStar(SkPoint center, float outer_radius, int point_count,
                float rotation_radians, float inner_radius_ratio = 0.56F);

} // namespace geometry::shapes
