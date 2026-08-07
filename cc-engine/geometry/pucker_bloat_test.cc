#include "geometry/pucker_bloat.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "include/core/SkPathBuilder.h"

namespace {

bool Near(float first, float second, float tolerance = 0.001F) {
  return std::abs(first - second) <= tolerance;
}

bool ContainsOnlyCubics(const SkPath &path) {
  SkPath::RawIter iterator(path);
  SkPoint points[4];
  for (SkPath::Verb verb = iterator.next(points); verb != SkPath::kDone_Verb;
       verb = iterator.next(points)) {
    if (verb != SkPath::kMove_Verb && verb != SkPath::kCubic_Verb &&
        verb != SkPath::kClose_Verb) {
      return false;
    }
  }
  return true;
}

SkRect PointBounds(const std::vector<SkPoint> &points) {
  float minimum_x = INFINITY;
  float minimum_y = INFINITY;
  float maximum_x = -INFINITY;
  float maximum_y = -INFINITY;
  for (const SkPoint &point : points) {
    minimum_x = std::min(minimum_x, point.fX);
    minimum_y = std::min(minimum_y, point.fY);
    maximum_x = std::max(maximum_x, point.fX);
    maximum_y = std::max(maximum_y, point.fY);
  }
  return SkRect::MakeLTRB(minimum_x, minimum_y, maximum_x, maximum_y);
}

class PassthroughAlgorithm final : public geometry::PuckerBloatAlgorithm {
public:
  geometry::PuckerBloatResult
  Apply(const SkPath &input_path, const SkPoint &pivot, float,
        const geometry::PuckerBloatOptions &) const override {
    return {.path = input_path, .pivot = pivot};
  }
};

} // namespace

int main() {
  const SkPath empty;
  assert(geometry::PuckerBloat(empty).isEmpty());

  SkPathBuilder degenerate_builder;
  degenerate_builder.moveTo(5.0F, 5.0F).lineTo(5.0F, 10.0F);
  const SkPath degenerate = degenerate_builder.detach();
  assert(geometry::PuckerBloat(degenerate) == degenerate);

  SkPathBuilder square_builder;
  square_builder.addRect(SkRect::MakeXYWH(0.0F, 0.0F, 100.0F, 100.0F));
  SkPath square = square_builder.detach();
  square.setFillType(SkPathFillType::kEvenOdd);
  geometry::PuckerBloatOptions options;
  assert(geometry::PuckerBloat(square, options) == square);
  const geometry::PuckerBloatResult zero_result =
      geometry::PuckerBloatDetailed(square, options);
  assert(zero_result.path == square);
  assert(!zero_result.anchors.empty());
  assert(!zero_result.midpoints.empty());

  options.amount = 0.5F;
  const geometry::PuckerBloatResult bloated =
      geometry::PuckerBloatDetailed(square, options);
  assert(ContainsOnlyCubics(bloated.path));
  assert(bloated.path.getFillType() == SkPathFillType::kEvenOdd);
  const SkRect bloated_anchors = PointBounds(bloated.anchors);
  assert(Near(bloated_anchors.left(), 25.0F, 0.01F));
  assert(Near(bloated_anchors.right(), 75.0F, 0.01F));

  // The top edge's split midpoint moves from (50, 0) to (50, -25). Its
  // original inner handles are 16 2/3 units from the midpoint, so the 1.5x
  // radial-distance ratio must lengthen both offsets to 25 units.
  SkPath::RawIter bloated_iterator(bloated.path);
  SkPoint bloated_points[4];
  assert(bloated_iterator.next(bloated_points) == SkPath::kMove_Verb);
  assert(bloated_iterator.next(bloated_points) == SkPath::kCubic_Verb);
  assert(Near(bloated_points[2].fX, 25.0F, 0.01F));
  assert(Near(bloated_points[2].fY, -25.0F, 0.01F));
  assert(Near(bloated_points[3].fX, 50.0F, 0.01F));
  assert(Near(bloated_points[3].fY, -25.0F, 0.01F));
  assert(bloated_iterator.next(bloated_points) == SkPath::kCubic_Verb);
  assert(Near(bloated_points[1].fX, 75.0F, 0.01F));
  assert(Near(bloated_points[1].fY, -25.0F, 0.01F));

  options.amount = 1.0F;
  const geometry::PuckerBloatResult full_bloat =
      geometry::PuckerBloatDetailed(square, options);
  const SkRect collapsed_anchors = PointBounds(full_bloat.anchors);
  assert(Near(collapsed_anchors.left(), 50.0F, 0.01F));
  assert(Near(collapsed_anchors.right(), 50.0F, 0.01F));

  options.displacement_scale = 0.5F;
  const geometry::PuckerBloatResult capped_bloat =
      geometry::PuckerBloatDetailed(square, options);
  const SkRect capped_anchors = PointBounds(capped_bloat.anchors);
  assert(Near(capped_anchors.left(), 25.0F, 0.01F));
  assert(Near(capped_anchors.right(), 75.0F, 0.01F));

  options.displacement_scale = 1.0F;
  options.amount = -1.0F;
  const geometry::PuckerBloatResult puckered =
      geometry::PuckerBloatDetailed(square, options);
  assert(ContainsOnlyCubics(puckered.path));
  assert(!puckered.path.isEmpty());
  const SkRect expanded_anchors = PointBounds(puckered.anchors);
  assert(Near(expanded_anchors.left(), -50.0F, 0.01F));
  assert(Near(expanded_anchors.right(), 150.0F, 0.01F));
  const SkRect collapsed_midpoints = PointBounds(puckered.midpoints);
  assert(Near(collapsed_midpoints.left(), 50.0F, 0.01F));
  assert(Near(collapsed_midpoints.right(), 50.0F, 0.01F));

  SkPathBuilder triangle_builder;
  triangle_builder.moveTo(0.0F, 0.0F);
  triangle_builder.lineTo(100.0F, 0.0F);
  triangle_builder.lineTo(0.0F, 100.0F);
  triangle_builder.close();
  geometry::PuckerBloatOptions triangle_options;
  triangle_options.amount = 0.25F;
  const geometry::PuckerBloatResult triangle = geometry::PuckerBloatDetailed(
      triangle_builder.detach(), triangle_options);
  assert(Near(triangle.pivot.fX, 100.0F / 3.0F, 0.01F));
  assert(Near(triangle.pivot.fY, 100.0F / 3.0F, 0.01F));

  SkPathBuilder mixed_builder;
  mixed_builder.moveTo(0.0F, 0.0F);
  mixed_builder.quadTo(50.0F, -30.0F, 100.0F, 0.0F);
  mixed_builder.conicTo(130.0F, 50.0F, 100.0F, 100.0F, 0.7F);
  mixed_builder.close();
  options.amount = 0.25F;
  options.pivot_mode = geometry::PuckerBloatPivotMode::kCustomPoint;
  options.custom_pivot = {25.0F, 50.0F};
  options.split_by_arc_length = true;
  options.arc_length_sample_count = 48;
  const SkPath mixed = geometry::PuckerBloat(mixed_builder.detach(), options);
  assert(ContainsOnlyCubics(mixed));
  assert(mixed.isFinite());

  options.custom_pivot = {INFINITY, 0.0F};
  const SkPath original = square;
  assert(geometry::PuckerBloat(original, options) == original);

  options.custom_pivot = {12.0F, 34.0F};
  const PassthroughAlgorithm passthrough;
  const geometry::PuckerBloatResult plugged =
      geometry::PuckerBloatDetailed(original, options, &passthrough);
  assert(plugged.path == original);
  assert(Near(plugged.pivot.fX, 12.0F));
  assert(Near(plugged.pivot.fY, 34.0F));
  return 0;
}
