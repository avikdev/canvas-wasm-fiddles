#include "fiddles/orbital_bloom_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>

void OrbitalBloomFiddle::Render(double time_seconds) {
  emscripten::val& context = Context();
  const double width = Width();
  const double height = Height();
  const double center_x = width / 2.0;
  const double center_y = height / 2.0;
  const double orbit = std::min(width, height) * 0.27;

  context.set("fillStyle", std::string("#17201c"));
  context.call<void>("fillRect", 0.0, 0.0, width, height);

  context.set("strokeStyle", std::string("rgba(246, 243, 234, 0.12)"));
  context.set("lineWidth", 1.0);
  for (int ring = 1; ring <= 4; ++ring) {
    context.call<void>("beginPath");
    context.call<void>("arc", center_x, center_y,
                       orbit * static_cast<double>(ring) / 4.0, 0.0,
                       std::numbers::pi * 2.0);
    context.call<void>("stroke");
  }

  constexpr std::array<const char*, 4> colors = {
      "#c7f36b", "#7de2ba", "#ff8066", "#7ca6ff"};
  for (int index = 0; index < 28; ++index) {
    const int lane = (index % 4) + 1;
    const double angle =
        time_seconds * (0.22 + static_cast<double>(lane) * 0.025) +
        static_cast<double>(index) * 1.73;
    const double radius = orbit * static_cast<double>(lane) / 4.0;
    const double x = center_x + std::cos(angle) * radius;
    const double y = center_y + std::sin(angle * 1.07) * radius;
    const double dot_radius = 3.0 + static_cast<double>((index * 7) % 9);
    const double alpha =
        0.56 +
        (std::sin(time_seconds * 2.0 + static_cast<double>(index)) + 1.0) *
            0.2;

    context.set("globalAlpha", alpha);
    context.set("fillStyle", std::string(colors[index % colors.size()]));
    context.call<void>("beginPath");
    context.call<void>("arc", x, y, dot_radius, 0.0,
                       std::numbers::pi * 2.0);
    context.call<void>("fill");
  }

  context.set("globalAlpha", 1.0);
  context.set("fillStyle", std::string("#f6f3ea"));
  context.call<void>("beginPath");
  context.call<void>("arc", center_x, center_y,
                     13.0 + std::sin(time_seconds * 2.4) * 2.0, 0.0,
                     std::numbers::pi * 2.0);
  context.call<void>("fill");
}
