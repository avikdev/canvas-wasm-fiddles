#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "morphing/geometry_kernel.h"

namespace skmorph::internal {

// Parsed closed contour with cached arc-length and area metadata. This is the
// bridge between native SkPath verbs and the geometry kernel.
class Contour {
public:
  struct Span {
    geometry::Curve curve = geometry::Curve::Line({0.0F, 0.0F}, {0.0F, 0.0F});
    geometry::ArcLengthTable arc_lengths;
    float start_length = 0.0F;
    float end_length = 0.0F;
  };

  explicit Contour(std::vector<geometry::Curve> curves);

  bool valid() const;
  const std::vector<Span> &spans() const;
  float length() const;
  float signed_area() const;
  SkPoint center() const;
  SkRect bounds() const;

  void Reverse();
  void RotateStart(float normalized_length);
  float ExtremePosition(SkVector direction) const;
  float NearestPosition(SkPoint point) const;

  std::vector<float> MandatoryPositions(uint32_t subdivision_flags) const;
  geometry::Curve CurveBetween(float normalized_start,
                               float normalized_end) const;

private:
  struct Location {
    size_t span_index = 0;
    float parameter = 0.0F;
  };

  Location Locate(float normalized_length) const;
  void RebuildMetrics();

  std::vector<Span> spans_;
  float length_ = 0.0F;
  float signed_area_ = 0.0F;
  SkPoint center_ = {0.0F, 0.0F};
  SkRect bounds_ = SkRect::MakeEmpty();
};

struct ParsedShape {
  std::vector<Contour> contours;
  size_t outer_index = 0;
  SkPathFillType fill_type = SkPathFillType::kWinding;
  SkRect bounds = SkRect::MakeEmpty();
};

// Parses exactly one non-degenerate outer contour and zero or more holes.
// Open contours, non-finite data, inverse fill types, and multiple disjoint
// outer contours are rejected with a diagnostic suitable for ShapeMorpher.
bool ParseClosedShape(const SkPath &path, ParsedShape *shape,
                      std::string *error);

} // namespace skmorph::internal
