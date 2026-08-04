#include "utils/perlin_noise.h"

#include <cmath>
#include <iostream>
#include <limits>

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
  const float first = noise::Perlin2D(1.25F, -3.75F, 42U);
  const float second = noise::Perlin2D(1.25F, -3.75F, 42U);
  success &= Expect(first == second, "Perlin noise should be deterministic.");
  success &= Expect(first >= -1.0F && first <= 1.0F,
                    "Perlin noise should remain in its documented range.");
  success &= Expect(noise::Perlin2D(3.0F, 8.0F, 7U) == 0.0F,
                    "Noise at an integer lattice point should be zero.");
  success &=
      Expect(noise::FractalPerlin2D(0.3F, 0.8F, 4, 2.0F, 0.5F, 9U) >= -1.0F &&
                 noise::FractalPerlin2D(0.3F, 0.8F, 4, 2.0F, 0.5F, 9U) <= 1.0F,
             "Fractal noise should remain normalized.");
  const float first_3d = noise::Perlin3D(1.25F, -3.75F, 0.5F, 42U);
  const float second_3d = noise::Perlin3D(1.25F, -3.75F, 0.5F, 42U);
  success &=
      Expect(first_3d == second_3d, "3D Perlin noise should be deterministic.");
  success &= Expect(first_3d >= -1.0F && first_3d <= 1.0F,
                    "3D Perlin noise should remain normalized.");
  success &= Expect(noise::Perlin3D(3.0F, 8.0F, -2.0F, 7U) == 0.0F,
                    "3D noise at an integer lattice point should be zero.");
  const float fractal_3d =
      noise::FractalPerlin3D(0.3F, 0.8F, 0.2F, 4, 2.0F, 0.5F, 9U);
  success &= Expect(fractal_3d >= -1.0F && fractal_3d <= 1.0F,
                    "Fractal 3D noise should remain normalized.");
  success &= Expect(
      noise::Perlin2D(std::numeric_limits<float>::infinity(), 0.0F) == 0.0F,
      "Invalid coordinates should return zero.");
  success &=
      Expect(noise::Perlin2D(std::numeric_limits<float>::max(), 0.0F) == 0.0F,
             "Coordinates outside the safe lattice range should return zero.");
  success &=
      Expect(noise::Perlin3D(0.0F, 0.0F,
                             std::numeric_limits<float>::infinity()) == 0.0F,
             "Non-finite 3D coordinates should return zero.");
  return success ? 0 : 1;
}
