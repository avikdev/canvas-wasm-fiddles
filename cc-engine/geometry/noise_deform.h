#pragma once

#include <cstdint>

#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

namespace geometry {

struct NoiseDeformParameters {
  float time = 0.0F;
  float horizontal_amplitude = 0.0F;
  float vertical_amplitude = 0.0F;
  float horizontal_frequency = 3.2F;
  float vertical_frequency = 15.0F;
  std::uint32_t seed = 0U;
};

// Returns the moving effect box used by the Noise Deform fiddle. The box is
// always 60% of the canvas width and 40% of its height. Its bottom stays
// between 3% and 18% above the canvas bottom.
SkRect AnimatedNoiseEffectBox(float width, float height, float time);

// Applies an anisotropic animated 3D Perlin field in coordinates local to the
// effect box. The caller decides which points are affected, so displaced
// points are intentionally allowed to leave the box.
SkPoint DeformPointWithNoise(const SkPoint &point, const SkRect &effect_box,
                             const NoiseDeformParameters &parameters);

} // namespace geometry
