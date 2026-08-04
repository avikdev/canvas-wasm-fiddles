#include "geometry/marching_squares.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

} // namespace

int main() {
  bool success = true;

  geometry::ScalarGrid invalid;
  success &=
      Expect(geometry::ExtractMarchingSquaresContours(invalid, 0.5F).empty(),
             "Invalid grids should not produce contours.");

  geometry::ScalarGrid plane;
  plane.bounds = SkRect::MakeWH(2.0F, 2.0F);
  plane.column_count = 3;
  plane.row_count = 3;
  plane.values = {
      0.0F, 0.5F, 1.0F, 0.0F, 0.5F, 1.0F, 0.0F, 0.5F, 1.0F,
  };
  const std::vector<geometry::ContourPolyline> plane_contours =
      geometry::ExtractMarchingSquaresContours(plane, 0.25F);
  success &= Expect(plane_contours.size() == 1U,
                    "Matching cell segments should stitch into one path.");
  success &= Expect(!plane_contours.empty() && !plane_contours[0].closed,
                    "A contour crossing the domain should remain open.");
  success &=
      Expect(!plane_contours.empty() && plane_contours[0].points.size() == 3U,
             "The stitched plane contour should retain its three "
             "ordered edge intersections.");
  if (!plane_contours.empty()) {
    for (const SkPoint &point : plane_contours[0].points) {
      success &= Expect(std::abs(point.fX - 0.5F) < 0.0001F,
                        "Linear edge interpolation should be exact.");
    }
  }

  geometry::ScalarGrid peak;
  peak.bounds = SkRect::MakeWH(2.0F, 2.0F);
  peak.column_count = 3;
  peak.row_count = 3;
  peak.values = {
      0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F,
  };
  const std::vector<geometry::ContourPolyline> peak_contours =
      geometry::ExtractMarchingSquaresContours(peak, 0.5F);
  success &= Expect(peak_contours.size() == 1U,
                    "An isolated peak should produce one contour.");
  success &= Expect(!peak_contours.empty() && peak_contours[0].closed,
                    "An isolated peak contour should close.");
  success &=
      Expect(!peak_contours.empty() && peak_contours[0].points.size() == 4U,
             "The peak contour should stitch into a four-point loop.");

  return success ? 0 : 1;
}
