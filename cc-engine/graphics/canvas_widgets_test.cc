#include "graphics/canvas_widgets.h"

#include <cassert>
#include <cmath>

namespace {

bool Near(float first, float second) {
  return std::abs(first - second) < 0.001F;
}

} // namespace

int main() {
  const SkRect track = SkRect::MakeXYWH(10.0F, 5.0F, 100.0F, 8.0F);
  const auto zero =
      graphics::canvas_widgets::MakeTwoSidedSliderGeometry(track, 0.0F);
  assert(Near(zero.center_x, 60.0F));
  assert(Near(zero.marker_x, 60.0F));
  assert(zero.negative_fill.isEmpty() && zero.positive_fill.isEmpty());

  const auto negative =
      graphics::canvas_widgets::MakeTwoSidedSliderGeometry(track, -0.5F);
  assert(Near(negative.marker_x, 35.0F));
  assert(Near(negative.negative_fill.left(), 35.0F));
  assert(Near(negative.negative_fill.right(), 60.0F));

  const auto positive =
      graphics::canvas_widgets::MakeTwoSidedSliderGeometry(track, 2.0F);
  assert(Near(positive.marker_x, 110.0F));
  assert(Near(positive.positive_fill.left(), 60.0F));
  assert(Near(positive.positive_fill.right(), 110.0F));

  const SkRect half =
      graphics::canvas_widgets::MakeOneSidedSliderFill(track, 0.5F);
  assert(Near(half.left(), 10.0F));
  assert(Near(half.right(), 60.0F));
  const SkRect clamped =
      graphics::canvas_widgets::MakeOneSidedSliderFill(track, 2.0F);
  assert(Near(clamped.right(), 110.0F));
  return 0;
}
