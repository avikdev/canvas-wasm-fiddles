#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"

namespace skmorph {

enum class StartPointMode {
  // Selects corresponding support points along the line between contour
  // centres, falling back to the positive X direction for coincident centres.
  kAutomatic,
  // Preserves the first explicit vertex of every input contour.
  kFirstVerb,
  // Uses the closest points to sourceStartPoint and targetStartPoint for the
  // outer contours. Holes continue to use automatic alignment.
  kUserSpecified,
};

enum MorphSubdivisionFlags : uint32_t {
  kSplitFlatness = 1U << 0U,
  kSplitTangentAngle = 1U << 1U,
  kSplitCurvature = 1U << 2U,
  kSplitInflection = 1U << 3U,
  kSplitCusps = 1U << 4U,
  kSplitArcLengthError = 1U << 5U,
  kSplitTurningAngle = 1U << 6U,
  // Caps physical distance as well as geometric complexity. This is important
  // for long straight glyph edges, which all other adaptive tests accept.
  kSplitMaxArcLength = 1U << 7U,
};

struct MorphOptions {
  // Enables the revision-03 feature-pivot correspondence solver. False keeps
  // the original synchronized normalized-arc-length pipeline byte-for-byte.
  bool enableFeaturePivotCorrespondence = false;

  // Correspondence-only rotations, applied around each shape's bounds centre
  // before start-point selection. They are reflected in the returned endpoints.
  float sourceRotationDegrees = 0.0F;
  float targetRotationDegrees = 0.0F;

  StartPointMode startPointMode = StartPointMode::kAutomatic;
  std::optional<SkPoint> sourceStartPoint;
  std::optional<SkPoint> targetStartPoint;

  uint32_t subdivisionFlags = kSplitFlatness | kSplitTangentAngle |
                              kSplitCurvature | kSplitInflection | kSplitCusps |
                              kSplitMaxArcLength;

  float flatnessEpsilon = 0.25F;
  float tangentAngleEpsilonDegrees = 8.0F;
  float curvatureEpsilon = 0.02F;
  float arcLengthEpsilon = 0.10F;
  float turningAngleDegrees = 30.0F;
  float maximumSegmentArcLength = 72.0F;

  // A hole is locally deformed at interpolation time when it approaches the
  // current outer contour more closely than this distance. Endpoint clearance
  // is never increased beyond what the input shapes already provide.
  float holeClearanceEpsilon = 1.0F;

  // Hard safety bounds. Hitting either bound accepts the current interval,
  // guaranteeing deterministic memory and runtime for adversarial paths.
  int maximumSubdivisionDepth = 14;
  int maximumSegmentsPerContour = 4096;
};

// Start-point diagnostics for one output contour. original is the first point
// from the input path before cyclic correspondence alignment. shifted is
// present only when the selected canonical start differs at the requested t.
struct ContourStartPoints {
  SkPoint original = {0.0F, 0.0F};
  std::optional<SkPoint> shifted;
  bool isHole = false;
};

// Stateful A-to-B shape morpher. Init performs all expensive parsing,
// correspondence, arc-length work, and adaptive subdivision. GetMorphed only
// interpolates prepared cubic control points and rebuilds a native SkPath.
//
// The supported topology is one closed outer contour plus zero or more holes.
// Extra disjoint outer contours and open/inverse-filled paths are rejected.
class ShapeMorpher {
public:
  ShapeMorpher();
  ~ShapeMorpher();

  ShapeMorpher(ShapeMorpher &&) noexcept;
  ShapeMorpher &operator=(ShapeMorpher &&) noexcept;
  ShapeMorpher(const ShapeMorpher &) = delete;
  ShapeMorpher &operator=(const ShapeMorpher &) = delete;

  bool Init(const SkPath &source, const SkPath &target,
            const MorphOptions &options = {});

  // t is clamped to [0, 1]. A non-finite t is treated as zero. An empty path is
  // returned when initialization has not succeeded.
  SkPath GetMorphed(float t) const;

  // Returns one entry for the outer contour followed by the currently visible
  // holes. Coordinates use the same space as GetMorphed(t).
  std::vector<ContourStartPoints> GetStartPoints(float t) const;

  bool isInitialized() const;
  size_t contourCount() const;
  size_t segmentCount() const;
  const std::string &error() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace skmorph
