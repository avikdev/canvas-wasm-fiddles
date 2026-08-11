#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "include/core/SkPath.h"

namespace geometry {

using WidthProfile = std::function<float(float)>;

// Returns uniformly spaced fractions in [0, 1]. `split_count` is the number
// of equal intervals, so including both endpoints returns split_count + 1
// values. Endpoint exclusion defaults to the useful stamping behavior.
// TODO: Support caller-provided non-uniform spacing policies.
std::vector<float> GenerateUniformSplitPoints(int split_count,
                                              bool exclude_start = true,
                                              bool exclude_end = true);

// Prepares a pair of corresponding paths and stamps their interpolation along
// a guide path. Source paths may be either open single contours or compatible
// closed shapes. Expensive path parsing, arc-length correspondence, and guide
// measurement happen in Init; GetCurve only interpolates and places a curve.
//
// For normalized guide length l, the interpolated curve's linearly
// interpolated bounding box is centered at guide(l). Its local north axis is
// rotated onto the guide tangent. The box width is then multiplied by
// width_profile(l); the profile must return a finite value greater than zero.
//
// Example:
//   CurveInterpolate curves;
//   curves.Init(a, b, guide, [](float l) {
//     return 1.0F + 0.3F * std::sin(4.0F * std::numbers::pi_v<float> * l);
//   });
//   for (float l : GenerateUniformSplitPoints(80, false, false)) {
//     canvas->drawPath(curves.GetCurve(l), paint);
//   }
class CurveInterpolate {
public:
  CurveInterpolate();
  ~CurveInterpolate();

  CurveInterpolate(CurveInterpolate &&) noexcept;
  CurveInterpolate &operator=(CurveInterpolate &&) noexcept;
  CurveInterpolate(const CurveInterpolate &) = delete;
  CurveInterpolate &operator=(const CurveInterpolate &) = delete;

  bool Init(const SkPath &start_curve, const SkPath &end_curve,
            const SkPath &guide_path, WidthProfile width_profile);

  // l is clamped to [0, 1]. Returns an empty path if Init failed or if the
  // width profile violates its contract at l.
  SkPath GetCurve(float l) const;

  bool isInitialized() const;
  float guideLength() const;
  const std::string &error() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace geometry
