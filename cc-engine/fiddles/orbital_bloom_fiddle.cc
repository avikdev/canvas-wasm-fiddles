#include "fiddles/orbital_bloom_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>

void OrbitalBloomFiddle::Render(double time_seconds) {
  emscripten::val &context = Context();
  const double width = Width();
  const double height = Height();
  const double center_x = width / 2.0;
  const double center_y = height / 2.0;
  const double orbit = std::min(width, height) * 0.43;

  context.set("fillStyle", std::string("#17201c"));
  context.call<void>("fillRect", 0.0, 0.0, width, height);

  context.set("strokeStyle", std::string("rgba(246, 243, 234, 0.12)"));
  context.set("lineWidth", 1.0);
  constexpr int kOrbitCount = 10;
  for (int lane = 0; lane < kOrbitCount; ++lane) {
    const double ratio = 0.24 + static_cast<double>(lane) /
                                    static_cast<double>(kOrbitCount - 1) * 0.76;
    const double radius_x = orbit * ratio;
    const double radius_y =
        radius_x * (0.38 + static_cast<double>((lane * 7) % 5) * 0.095);
    const double rotation = -0.72 + static_cast<double>(lane) * 0.17;
    context.call<void>("beginPath");
    context.call<void>("ellipse", center_x, center_y, radius_x, radius_y,
                       rotation, 0.0, std::numbers::pi * 2.0);
    context.call<void>("stroke");
  }

  constexpr std::array<const char *, 4> colors = {"#c7f36b", "#7de2ba",
                                                  "#ff8066", "#7ca6ff"};
  for (int lane = 0; lane < kOrbitCount; ++lane) {
    const int particle_count = 2 + (lane % 2);
    const double ratio = 0.24 + static_cast<double>(lane) /
                                    static_cast<double>(kOrbitCount - 1) * 0.76;
    const double radius_x = orbit * ratio;
    const double radius_y =
        radius_x * (0.38 + static_cast<double>((lane * 7) % 5) * 0.095);
    const double rotation = -0.72 + static_cast<double>(lane) * 0.17;
    const double cosine_rotation = std::cos(rotation);
    const double sine_rotation = std::sin(rotation);

    for (int particle = 0; particle < particle_count; ++particle) {
      const int index = lane * 3 + particle;
      const double angle =
          time_seconds * (0.19 + static_cast<double>(lane) * 0.013) +
          static_cast<double>(particle) * std::numbers::pi * 2.0 /
              static_cast<double>(particle_count) +
          static_cast<double>(lane) * 0.83;
      const double local_x = std::cos(angle) * radius_x;
      const double local_y = std::sin(angle) * radius_y;
      const double x =
          center_x + local_x * cosine_rotation - local_y * sine_rotation;
      const double y =
          center_y + local_x * sine_rotation + local_y * cosine_rotation;
      double dot_radius = 3.0 + static_cast<double>((index * 7) % 8);
      if (index % 9 == 0) {
        dot_radius += 11.0;
      } else if (index % 7 == 0) {
        dot_radius += 6.0;
      }
      const double alpha =
          0.58 +
          (std::sin(time_seconds * 2.0 + static_cast<double>(index)) + 1.0) *
              0.19;

      context.set("globalAlpha", alpha);
      context.set("fillStyle", std::string(colors[index % colors.size()]));
      context.call<void>("beginPath");
      context.call<void>("arc", x, y, dot_radius, 0.0, std::numbers::pi * 2.0);
      context.call<void>("fill");
    }
  }

  context.set("globalAlpha", 1.0);
  context.set("fillStyle", std::string("#f6f3ea"));
  context.call<void>("beginPath");
  context.call<void>("arc", center_x, center_y,
                     13.0 + std::sin(time_seconds * 2.4) * 2.0, 0.0,
                     std::numbers::pi * 2.0);
  context.call<void>("fill");
}
