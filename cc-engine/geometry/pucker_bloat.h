#pragma once

#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"

namespace geometry {

enum class PuckerBloatPivotMode {
  kGeometricCenter,
  kCustomPoint,
};

struct PuckerBloatOptions {
  // Normalized deformation in [-1, 1]. Positive values bloat by moving
  // original anchors toward the pivot and split midpoints away. Negative
  // values pucker by reversing those motions.
  float amount = 0.0F;
  // Multiplies amount before geometry is displaced. The default preserves the
  // full normalized range; clients can cap visual intensity independently
  // while keeping their UI value in [-1, 1].
  float displacement_scale = 1.0F;
  PuckerBloatPivotMode pivot_mode = PuckerBloatPivotMode::kGeometricCenter;
  SkPoint custom_pivot = {0.0F, 0.0F};

  // False splits every normalized cubic at parameter t=0.5. True estimates
  // the parameter whose sampled arc length is half the segment length.
  bool split_by_arc_length = false;
  int arc_length_sample_count = 32;
};

struct PuckerBloatResult {
  SkPath path;
  SkPoint pivot = {0.0F, 0.0F};
  // Final positions used by the fiddle's deformation guides.
  std::vector<SkPoint> anchors;
  std::vector<SkPoint> midpoints;
};

// Strategy interface for algorithms sharing the same path, pivot, amount, and
// guide-result setup. A future algorithm can be plugged into
// PuckerBloatDetailed without changing callers or fiddle layout code.
class PuckerBloatAlgorithm {
public:
  virtual ~PuckerBloatAlgorithm() = default;

  virtual PuckerBloatResult Apply(const SkPath &input_path,
                                  const SkPoint &pivot, float amount,
                                  const PuckerBloatOptions &options) const = 0;
};

// Current algorithm: normalize segments to cubics, split each segment in two,
// move original anchors and split midpoints in opposite radial directions, and
// reconstruct the paired cubics with collinear inner controls whose lengths
// scale with the split midpoint's radial displacement.
const PuckerBloatAlgorithm &SplitSegmentPuckerBloatAlgorithm();

PuckerBloatResult
PuckerBloatDetailed(const SkPath &input_path,
                    const PuckerBloatOptions &options = {},
                    const PuckerBloatAlgorithm *algorithm = nullptr);

// Convenience path-only API. Zero amount and invalid/degenerate inputs return
// the original path exactly.
SkPath PuckerBloat(const SkPath &input_path,
                   const PuckerBloatOptions &options = {});

} // namespace geometry
