#pragma once

#include <cstdint>

namespace noise {

// Deterministic 2D gradient Perlin noise in approximately [-1, 1].
float Perlin2D(float x, float y, std::uint32_t seed = 0U);

// Deterministic 3D gradient Perlin noise in approximately [-1, 1]. The third
// coordinate is suitable for time-varying slices through a coherent field.
float Perlin3D(float x, float y, float z, std::uint32_t seed = 0U);

// Sums successive octaves with configurable lacunarity and persistence.
// Invalid inputs return zero; octave_count is clamped to [1, 12].
float FractalPerlin2D(float x, float y, int octave_count,
                      float lacunarity = 2.0F, float persistence = 0.5F,
                      std::uint32_t seed = 0U);

float FractalPerlin3D(float x, float y, float z, int octave_count,
                      float lacunarity = 2.0F, float persistence = 0.5F,
                      std::uint32_t seed = 0U);

} // namespace noise
