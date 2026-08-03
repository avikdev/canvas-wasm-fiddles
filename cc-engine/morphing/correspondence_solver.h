#pragma once

#include <cstdint>
#include <vector>

#include "include/core/SkPoint.h"

namespace skmorph::correspondence {

enum PivotFeature : uint32_t {
  kOrdinary = 0U,
  kOriginalVerb = 1U << 0U,
  kSharpCorner = 1U << 1U,
  kCurvatureExtremum = 1U << 2U,
  kInflection = 1U << 3U,
  kCusp = 1U << 4U,
  kTangentDiscontinuity = 1U << 5U,
  kLongStraightEnd = 1U << 6U,
  kAxisExtremum = 1U << 7U,
};

// Geometry and feature metadata for one ordered contour pivot. position is
// normalized arc length in [0, 1]. The first and last pivots must be the
// duplicated endpoints of a closed contour.
struct Pivot {
  float position = 0.0F;
  SkPoint point = {0.0F, 0.0F};
  float radialAngle = 0.0F;
  float tangentAngle = 0.0F;
  float scaledCurvature = 0.0F;
  uint32_t features = kOrdinary;
};

struct Match {
  size_t sourceIndex = 0;
  size_t targetIndex = 0;
};

struct SolverOptions {
  float arcWeight = 1.0F;
  float radialAngleWeight = 1.25F;
  float tangentWeight = 0.75F;
  float curvatureWeight = 0.5F;
  float featureWeight = 6.0F;
  float ordinarySkipPenalty = 0.2F;
  float featureSkipPenalty = 12.0F;
};

// Finds a strictly monotonic sequence of matched pivots. Endpoints are always
// matched. Interior ordinary pivots may be skipped; skipping or incompatibly
// matching feature pivots carries a high cost. Empty means invalid input.
std::vector<Match> SolveMonotonic(const std::vector<Pivot> &source,
                                  const std::vector<Pivot> &target,
                                  const SolverOptions &options = {});

} // namespace skmorph::correspondence
