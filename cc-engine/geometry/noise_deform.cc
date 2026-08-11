#include "geometry/noise_deform.h"

#include <algorithm>
#include <cmath>

#include "utils/perlin_noise.h"

namespace geometry {

SkRect AnimatedNoiseEffectBox(float width, float height, float time) {
  if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(time) ||
      width <= 0.0F || height <= 0.0F) {
    return SkRect::MakeEmpty();
  }
  const float box_width = width * 0.60F;
  const float box_height = height * 0.40F;
  const float horizontal_wave = std::sin(time * 0.31F);
  const float vertical_wave = std::sin(time * 0.23F + 1.1F);
  const float center_x = width * (0.50F + horizontal_wave * 0.18F);
  const float bottom_clearance = height * (0.105F + vertical_wave * 0.075F);
  const float bottom = height - bottom_clearance;
  return SkRect::MakeXYWH(center_x - box_width * 0.5F, bottom - box_height,
                          box_width, box_height);
}

SkPoint DeformPointWithNoise(const SkPoint &point, const SkRect &effect_box,
                             const NoiseDeformParameters &parameters) {
  if (!effect_box.isFinite() || effect_box.isEmpty() ||
      !std::isfinite(point.fX) || !std::isfinite(point.fY)) {
    return point;
  }
  const float local_x = (point.fX - effect_box.left()) / effect_box.width();
  const float local_y = (point.fY - effect_box.top()) / effect_box.height();
  const float z = parameters.time * 0.22F;
  const float horizontal =
      noise::FractalPerlin3D(local_x * parameters.horizontal_frequency,
                             local_y * parameters.vertical_frequency, z, 3,
                             2.0F, 0.52F, parameters.seed);
  const float vertical = noise::FractalPerlin3D(
      local_x * (parameters.horizontal_frequency * 0.72F),
      local_y * (parameters.vertical_frequency * 0.82F), z + 13.7F, 2, 2.1F,
      0.48F, parameters.seed ^ 0x9e3779b9U);
  return {point.fX + horizontal * parameters.horizontal_amplitude,
          point.fY + vertical * parameters.vertical_amplitude};
}

} // namespace geometry
