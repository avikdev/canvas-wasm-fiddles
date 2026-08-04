#include "utils/perlin_noise.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace noise {
namespace {

std::uint32_t Hash(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

float Fade(float value) {
  return value * value * value * (value * (value * 6.0F - 15.0F) + 10.0F);
}

float GradientDot(int lattice_x, int lattice_y, float delta_x, float delta_y,
                  std::uint32_t seed) {
  const std::uint32_t hash =
      Hash(static_cast<std::uint32_t>(lattice_x) * 0x9e3779b9U ^
           static_cast<std::uint32_t>(lattice_y) * 0x85ebca6bU ^ seed);
  constexpr float kDiagonal = 0.70710678F;
  switch (hash & 7U) {
  case 0:
    return delta_x;
  case 1:
    return -delta_x;
  case 2:
    return delta_y;
  case 3:
    return -delta_y;
  case 4:
    return (delta_x + delta_y) * kDiagonal;
  case 5:
    return (delta_x - delta_y) * kDiagonal;
  case 6:
    return (-delta_x + delta_y) * kDiagonal;
  default:
    return (-delta_x - delta_y) * kDiagonal;
  }
}

float GradientDot3D(int lattice_x, int lattice_y, int lattice_z, float delta_x,
                    float delta_y, float delta_z, std::uint32_t seed) {
  const std::uint32_t hash =
      Hash(static_cast<std::uint32_t>(lattice_x) * 0x9e3779b9U ^
           static_cast<std::uint32_t>(lattice_y) * 0x85ebca6bU ^
           static_cast<std::uint32_t>(lattice_z) * 0xc2b2ae35U ^ seed);
  switch (hash % 12U) {
  case 0:
    return delta_x + delta_y;
  case 1:
    return delta_x - delta_y;
  case 2:
    return -delta_x + delta_y;
  case 3:
    return -delta_x - delta_y;
  case 4:
    return delta_x + delta_z;
  case 5:
    return delta_x - delta_z;
  case 6:
    return -delta_x + delta_z;
  case 7:
    return -delta_x - delta_z;
  case 8:
    return delta_y + delta_z;
  case 9:
    return delta_y - delta_z;
  case 10:
    return -delta_y + delta_z;
  default:
    return -delta_y - delta_z;
  }
}

} // namespace

float Perlin2D(float x, float y, std::uint32_t seed) {
  constexpr float kSafeLatticeLimit =
      static_cast<float>(std::numeric_limits<int>::max() - 2);
  if (!std::isfinite(x) || !std::isfinite(y) ||
      std::abs(x) > kSafeLatticeLimit || std::abs(y) > kSafeLatticeLimit) {
    return 0.0F;
  }
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float u = Fade(tx);
  const float v = Fade(ty);
  const float top = std::lerp(GradientDot(x0, y0, tx, ty, seed),
                              GradientDot(x0 + 1, y0, tx - 1.0F, ty, seed), u);
  const float bottom =
      std::lerp(GradientDot(x0, y0 + 1, tx, ty - 1.0F, seed),
                GradientDot(x0 + 1, y0 + 1, tx - 1.0F, ty - 1.0F, seed), u);
  return std::clamp(std::lerp(top, bottom, v) * 1.41421356F, -1.0F, 1.0F);
}

float Perlin3D(float x, float y, float z, std::uint32_t seed) {
  constexpr float kSafeLatticeLimit =
      static_cast<float>(std::numeric_limits<int>::max() - 2);
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      std::abs(x) > kSafeLatticeLimit || std::abs(y) > kSafeLatticeLimit ||
      std::abs(z) > kSafeLatticeLimit) {
    return 0.0F;
  }
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int z0 = static_cast<int>(std::floor(z));
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float tz = z - static_cast<float>(z0);
  const float u = Fade(tx);
  const float v = Fade(ty);
  const float w = Fade(tz);

  const auto gradient = [&](int dx, int dy, int dz) {
    return GradientDot3D(x0 + dx, y0 + dy, z0 + dz, tx - static_cast<float>(dx),
                         ty - static_cast<float>(dy),
                         tz - static_cast<float>(dz), seed);
  };
  const float z0_top = std::lerp(gradient(0, 0, 0), gradient(1, 0, 0), u);
  const float z0_bottom = std::lerp(gradient(0, 1, 0), gradient(1, 1, 0), u);
  const float z1_top = std::lerp(gradient(0, 0, 1), gradient(1, 0, 1), u);
  const float z1_bottom = std::lerp(gradient(0, 1, 1), gradient(1, 1, 1), u);
  const float near_slice = std::lerp(z0_top, z0_bottom, v);
  const float far_slice = std::lerp(z1_top, z1_bottom, v);
  return std::clamp(std::lerp(near_slice, far_slice, w) * 0.70710678F, -1.0F,
                    1.0F);
}

float FractalPerlin2D(float x, float y, int octave_count, float lacunarity,
                      float persistence, std::uint32_t seed) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(lacunarity) ||
      !std::isfinite(persistence) || lacunarity <= 0.0F || persistence < 0.0F) {
    return 0.0F;
  }
  octave_count = std::clamp(octave_count, 1, 12);
  float sum = 0.0F;
  float amplitude = 1.0F;
  float amplitude_sum = 0.0F;
  float frequency = 1.0F;
  for (int octave = 0; octave < octave_count; ++octave) {
    sum += Perlin2D(x * frequency, y * frequency,
                    seed + static_cast<std::uint32_t>(octave) * 0x9e3779b9U) *
           amplitude;
    amplitude_sum += amplitude;
    amplitude *= persistence;
    frequency *= lacunarity;
  }
  return amplitude_sum > 0.0F ? sum / amplitude_sum : 0.0F;
}

float FractalPerlin3D(float x, float y, float z, int octave_count,
                      float lacunarity, float persistence, std::uint32_t seed) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      !std::isfinite(lacunarity) || !std::isfinite(persistence) ||
      lacunarity <= 0.0F || persistence < 0.0F) {
    return 0.0F;
  }
  octave_count = std::clamp(octave_count, 1, 12);
  float sum = 0.0F;
  float amplitude = 1.0F;
  float amplitude_sum = 0.0F;
  float frequency = 1.0F;
  for (int octave = 0; octave < octave_count; ++octave) {
    sum += Perlin3D(x * frequency, y * frequency, z * frequency,
                    seed + static_cast<std::uint32_t>(octave) * 0x9e3779b9U) *
           amplitude;
    amplitude_sum += amplitude;
    amplitude *= persistence;
    frequency *= lacunarity;
  }
  return amplitude_sum > 0.0F ? sum / amplitude_sum : 0.0F;
}

} // namespace noise
