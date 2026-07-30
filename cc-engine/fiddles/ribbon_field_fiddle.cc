#include "fiddles/ribbon_field_fiddle.h"

#include <array>
#include <cmath>
#include <string>

void RibbonFieldFiddle::Render(double time_seconds) {
  emscripten::val& context = Context();
  const double width = Width();
  const double height = Height();

  emscripten::val gradient = context.call<emscripten::val>(
      "createLinearGradient", 0.0, 0.0, width, height);
  gradient.call<void>("addColorStop", 0.0, std::string("#efe9ff"));
  gradient.call<void>("addColorStop", 0.5, std::string("#f6f3ea"));
  gradient.call<void>("addColorStop", 1.0, std::string("#e2f7ed"));
  context.set("fillStyle", gradient);
  context.call<void>("fillRect", 0.0, 0.0, width, height);

  constexpr std::array<const char*, 5> colors = {
      "#17201c", "#ff8066", "#7ca6ff", "#9776e8", "#7de2ba"};
  const double spacing = height / static_cast<double>(colors.size() + 1);

  for (std::size_t ribbon = 0; ribbon < colors.size(); ++ribbon) {
    context.call<void>("beginPath");
    bool first_point = true;
    for (double x = -20.0; x <= width + 20.0; x += 8.0) {
      const double wave =
          std::sin(x * 0.013 +
                   time_seconds *
                       (0.65 + static_cast<double>(ribbon) * 0.08) +
                   static_cast<double>(ribbon)) *
              34.0 +
          std::sin(x * 0.027 - time_seconds * 0.45) * 12.0;
      const double y =
          spacing * static_cast<double>(ribbon + 1) + wave;
      if (first_point) {
        context.call<void>("moveTo", x, y);
        first_point = false;
      } else {
        context.call<void>("lineTo", x, y);
      }
    }

    context.set("strokeStyle", std::string(colors[ribbon]));
    context.set("lineWidth", 8.0 + static_cast<double>(ribbon) * 2.0);
    context.set("lineCap", std::string("round"));
    context.set("globalAlpha", 0.8);
    context.call<void>("stroke");
  }
  context.set("globalAlpha", 1.0);
}
