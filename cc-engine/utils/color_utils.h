#pragma once

#include "include/core/SkColor.h"

namespace color_utils {

SkColor4f FromHsv(float hue_degrees, float saturation, float value,
                  float alpha = 1.0F);

SkColor AdjustHsv(SkColor color, float saturation_scale, float value_scale,
                  float minimum_saturation = 0.0F, float minimum_value = 0.0F);

} // namespace color_utils
