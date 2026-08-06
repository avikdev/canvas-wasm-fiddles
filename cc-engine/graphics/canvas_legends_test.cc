#include "graphics/canvas_legends.h"

#include <cassert>
#include <cmath>

namespace {

bool Near(float first, float second) {
  return std::abs(first - second) < 0.001F;
}

} // namespace

int main() {
  const SkRect track = SkRect::MakeXYWH(10.0F, 5.0F, 100.0F, 8.0F);
  const SkRect full = graphics::canvas_legends::RemainingFillRect(track, 1.0F);
  assert(Near(full.left(), 10.0F));
  assert(Near(full.right(), 110.0F));

  const SkRect half = graphics::canvas_legends::RemainingFillRect(track, 0.5F);
  assert(Near(half.left(), 60.0F));
  assert(Near(half.right(), 110.0F));

  const SkRect empty =
      graphics::canvas_legends::RemainingFillRect(track, -2.0F);
  assert(Near(empty.left(), 110.0F));
  assert(Near(empty.right(), 110.0F));
  return 0;
}
