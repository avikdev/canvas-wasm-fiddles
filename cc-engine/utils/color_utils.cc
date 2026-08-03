#include "utils/color_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace color_utils {
namespace {

struct Hsv {
  float hue_degrees;
  float saturation;
  float value;
};

Hsv ToHsv(SkColor color) {
  const float red = static_cast<float>(SkColorGetR(color)) / 255.0F;
  const float green = static_cast<float>(SkColorGetG(color)) / 255.0F;
  const float blue = static_cast<float>(SkColorGetB(color)) / 255.0F;
  const float maximum = std::max({red, green, blue});
  const float minimum = std::min({red, green, blue});
  const float delta = maximum - minimum;

  float hue = 0.0F;
  if (delta > 0.0001F) {
    if (maximum == red) {
      hue = std::fmod((green - blue) / delta, 6.0F);
    } else if (maximum == green) {
      hue = (blue - red) / delta + 2.0F;
    } else {
      hue = (red - green) / delta + 4.0F;
    }
    hue *= 60.0F;
    if (hue < 0.0F) {
      hue += 360.0F;
    }
  }

  return {
      hue,
      maximum <= 0.0F ? 0.0F : delta / maximum,
      maximum,
  };
}

std::uint8_t ToChannel(float value) {
  return static_cast<std::uint8_t>(
      std::round(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

} // namespace

SkColor4f FromHsv(float hue_degrees, float saturation, float value,
                  float alpha) {
  hue_degrees = std::fmod(hue_degrees, 360.0F);
  if (hue_degrees < 0.0F) {
    hue_degrees += 360.0F;
  }
  saturation = std::clamp(saturation, 0.0F, 1.0F);
  value = std::clamp(value, 0.0F, 1.0F);

  const float chroma = value * saturation;
  const float sector = hue_degrees / 60.0F;
  const float secondary =
      chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
  float red = 0.0F;
  float green = 0.0F;
  float blue = 0.0F;
  if (sector < 1.0F) {
    red = chroma;
    green = secondary;
  } else if (sector < 2.0F) {
    red = secondary;
    green = chroma;
  } else if (sector < 3.0F) {
    green = chroma;
    blue = secondary;
  } else if (sector < 4.0F) {
    green = secondary;
    blue = chroma;
  } else if (sector < 5.0F) {
    red = secondary;
    blue = chroma;
  } else {
    red = chroma;
    blue = secondary;
  }
  const float match = value - chroma;
  return {red + match, green + match, blue + match,
          std::clamp(alpha, 0.0F, 1.0F)};
}

SkColor4f FromHsl(float hue_degrees, float saturation, float lightness,
                  float alpha) {
  hue_degrees = std::fmod(hue_degrees, 360.0F);
  if (hue_degrees < 0.0F) {
    hue_degrees += 360.0F;
  }
  saturation = std::clamp(saturation, 0.0F, 1.0F);
  lightness = std::clamp(lightness, 0.0F, 1.0F);

  const float chroma = (1.0F - std::abs(2.0F * lightness - 1.0F)) * saturation;
  const float sector = hue_degrees / 60.0F;
  const float secondary =
      chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
  float red = 0.0F;
  float green = 0.0F;
  float blue = 0.0F;
  if (sector < 1.0F) {
    red = chroma;
    green = secondary;
  } else if (sector < 2.0F) {
    red = secondary;
    green = chroma;
  } else if (sector < 3.0F) {
    green = chroma;
    blue = secondary;
  } else if (sector < 4.0F) {
    green = secondary;
    blue = chroma;
  } else if (sector < 5.0F) {
    red = secondary;
    blue = chroma;
  } else {
    red = chroma;
    blue = secondary;
  }
  const float match = lightness - chroma * 0.5F;
  return {red + match, green + match, blue + match,
          std::clamp(alpha, 0.0F, 1.0F)};
}

SkColor AdjustHsv(SkColor color, float saturation_scale, float value_scale,
                  float minimum_saturation, float minimum_value) {
  const Hsv hsv = ToHsv(color);
  const float saturation = std::clamp(
      std::max(hsv.saturation * saturation_scale, minimum_saturation), 0.0F,
      1.0F);
  const float value =
      std::clamp(std::max(hsv.value * value_scale, minimum_value), 0.0F, 1.0F);
  const SkColor4f adjusted =
      FromHsv(hsv.hue_degrees, saturation, value,
              static_cast<float>(SkColorGetA(color)) / 255.0F);
  return SkColorSetARGB(ToChannel(adjusted.fA), ToChannel(adjusted.fR),
                        ToChannel(adjusted.fG), ToChannel(adjusted.fB));
}

} // namespace color_utils
