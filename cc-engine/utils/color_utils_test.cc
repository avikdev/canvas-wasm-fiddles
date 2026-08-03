#include "utils/color_utils.h"

#include <cmath>
#include <iostream>

namespace {

bool Near(float actual, float expected) {
  return std::abs(actual - expected) <= 0.0001F;
}

bool Expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

} // namespace

int main() {
  bool success = true;

  const SkColor4f red = color_utils::FromHsl(0.0F, 0.75F, 0.60F);
  success &= Expect(Near(red.fR, 0.90F) && Near(red.fG, 0.30F) &&
                        Near(red.fB, 0.30F) && Near(red.fA, 1.0F),
                    "HSL red should convert to the expected RGB channels.");

  const SkColor4f wrapped_blue =
      color_utils::FromHsl(-120.0F, 1.0F, 0.50F, 2.0F);
  success &=
      Expect(Near(wrapped_blue.fR, 0.0F) && Near(wrapped_blue.fG, 0.0F) &&
                 Near(wrapped_blue.fB, 1.0F) && Near(wrapped_blue.fA, 1.0F),
             "Hue should wrap and alpha should clamp.");

  return success ? 0 : 1;
}
