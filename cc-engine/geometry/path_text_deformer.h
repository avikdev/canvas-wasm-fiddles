#pragma once

#include <memory>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"

class SkPathMeasure;

namespace geometry {

struct PathFrame {
  SkPoint position = {0.0F, 0.0F};
  SkVector tangent = {1.0F, 0.0F};
  SkVector normal = {0.0F, 1.0F};
  float signed_curvature = 0.0F;
  float turn_angle = 0.0F;
};

struct PathTextDeformOptions {
  float minimum_segment_length = 0.25F;
  float maximum_segment_length = 2.0F;
  float flatness_tolerance = 0.35F;
  float curvature_probe = 2.0F;
  float corner_angle_threshold_degrees = 30.0F;
  // Arc-length interval over which a discontinuous corner orientation is
  // blended. Positions remain on the original guide.
  float corner_transition_length = 0.0F;
  bool protect_sharp_turns = true;
  // Fraction of the local curvature radius available to inner glyph edges.
  float inversion_safety = 0.80F;
};

// Arc-length path frame and nonlinear glyph mapping. The first contour of the
// guide is used; it may contain any SkPath primitive supported by
// SkPathMeasure.
class PathTextDeformer {
public:
  explicit PathTextDeformer(const SkPath &guide);
  ~PathTextDeformer();

  PathTextDeformer(const PathTextDeformer &) = delete;
  PathTextDeformer &operator=(const PathTextDeformer &) = delete;

  bool valid() const;
  float length() const;
  PathFrame FrameAt(float distance, const PathTextDeformOptions &options) const;
  SkPoint MapPoint(SkPoint glyph_point, float horizontal_offset,
                   const PathTextDeformOptions &options) const;
  SkPath Deform(const SkPath &glyph_outline, float horizontal_offset,
                const PathTextDeformOptions &options) const;

private:
  float NormalizeDistance(float distance) const;
  bool RawFrame(float distance, SkPoint *position, SkVector *tangent) const;
  bool SmoothedCornerTangent(float distance,
                             const PathTextDeformOptions &options,
                             SkVector *tangent) const;

  SkPath guide_;
  std::unique_ptr<SkPathMeasure> measure_;
  float length_ = 0.0F;
  bool closed_ = false;
};

} // namespace geometry
