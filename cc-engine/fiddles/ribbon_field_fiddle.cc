#include "fiddles/ribbon_field_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

void RibbonFieldFiddle::Render(double time_seconds) {
  emscripten::val &context = Context();
  const double width = Width();
  const double height = Height();

  constexpr std::array<const char *, 8> colors = {
      "#715660", "#846470", "#566071", "#727f96",
      "#c39f72", "#d5bf95", "#667762", "#879e82"};
  constexpr std::size_t kLineCount = colors.size() - 1;
  constexpr double kSampleStep = 10.0;
  const double spacing = height / static_cast<double>(colors.size());
  const double amplitude = std::min(28.0, spacing * 0.22);
  const std::size_t sample_count =
      static_cast<std::size_t>(std::ceil(width / kSampleStep)) + 1;
  std::array<std::vector<double>, kLineCount> boundaries;

  for (std::size_t line = 0; line < kLineCount; ++line) {
    boundaries[line].reserve(sample_count);
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
      const double x =
          std::min(width, static_cast<double>(sample) * kSampleStep);
      const double wave =
          std::sin(x * 0.0105 + time_seconds * (0.42 + line * 0.035) +
                   static_cast<double>(line) * 0.72) *
              amplitude +
          std::sin(x * 0.022 - time_seconds * 0.31 +
                   static_cast<double>(line) * 0.38) *
              amplitude * 0.34;
      boundaries[line].push_back(spacing * static_cast<double>(line + 1) +
                                 wave);
    }
  }

  for (std::size_t region = 0; region < colors.size(); ++region) {
    emscripten::val region_gradient = context.call<emscripten::val>(
        "createLinearGradient", 0.0, 0.0, width, 0.0);
    region_gradient.call<void>("addColorStop", 0.0,
                               std::string(colors[region]));
    region_gradient.call<void>(
        "addColorStop", 1.0,
        std::string(colors[std::min(region + 1, colors.size() - 1)]));
    context.set("fillStyle", region_gradient);
    context.call<void>("beginPath");
    if (region == 0) {
      context.call<void>("moveTo", 0.0, 0.0);
      context.call<void>("lineTo", width, 0.0);
    } else {
      const auto &top = boundaries[region - 1];
      context.call<void>("moveTo", 0.0, top.front());
      for (std::size_t sample = 1; sample < sample_count; ++sample) {
        context.call<void>(
            "lineTo",
            std::min(width, static_cast<double>(sample) * kSampleStep),
            top[sample]);
      }
    }

    if (region == colors.size() - 1) {
      context.call<void>("lineTo", width, height);
      context.call<void>("lineTo", 0.0, height);
    } else {
      const auto &bottom = boundaries[region];
      for (std::size_t sample = sample_count; sample-- > 0;) {
        context.call<void>(
            "lineTo",
            std::min(width, static_cast<double>(sample) * kSampleStep),
            bottom[sample]);
      }
    }
    context.call<void>("closePath");
    context.call<void>("fill");
  }
}
