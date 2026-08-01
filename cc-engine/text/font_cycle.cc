#include "text/font_cycle.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "text/skia_font_manager.h"

namespace text {

int CyclingFontIndex(double time_seconds, double interval_seconds) {
  if (interval_seconds <= 0.0) {
    return 0;
  }
  const double safe_time = std::max(0.0, time_seconds);
  const auto cycle =
      static_cast<std::size_t>(std::floor(safe_time / interval_seconds));
  return static_cast<int>(cycle % kFontChoices.size());
}

std::string ResolveFontFamily(const FontChoice &choice) {
  return choice.font_id == nullptr
             ? std::string()
             : SkiaFontManager::Instance().FamilyNameForId(choice.font_id);
}

} // namespace text
