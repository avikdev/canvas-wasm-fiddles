#include "geometry/noise_deform.h"

#include <cassert>
#include <cmath>

int main() {
  constexpr float width = 1000.0F;
  constexpr float height = 600.0F;
  for (float time : {0.0F, 2.0F, 7.0F, 19.0F}) {
    const SkRect box = geometry::AnimatedNoiseEffectBox(width, height, time);
    assert(std::abs(box.width() - width * 0.6F) < 0.001F);
    assert(std::abs(box.height() - height * 0.4F) < 0.001F);
    assert(box.left() >= 0.0F && box.right() <= width);
    assert(box.top() >= 0.0F && box.bottom() <= height);
    assert(height - box.bottom() <= height * 0.2F + 0.001F);
  }

  const SkRect box = geometry::AnimatedNoiseEffectBox(width, height, 1.0F);
  const SkPoint point = {box.centerX(), box.centerY()};
  geometry::NoiseDeformParameters parameters;
  parameters.time = 2.0F;
  parameters.horizontal_amplitude = 40.0F;
  parameters.vertical_amplitude = 12.0F;
  parameters.seed = 73U;
  const SkPoint first = geometry::DeformPointWithNoise(point, box, parameters);
  const SkPoint repeated =
      geometry::DeformPointWithNoise(point, box, parameters);
  assert(std::isfinite(first.fX) && std::isfinite(first.fY));
  assert(first == repeated);
  parameters.time += 1.0F;
  const SkPoint later = geometry::DeformPointWithNoise(point, box, parameters);
  assert(first != later);

  return 0;
}
