#pragma once

#include <array>
#include <string>

namespace text {

struct FontChoice {
  const char *font_id;
  const char *display_name;
};

inline constexpr std::array<FontChoice, 3> kFontChoices = {{
    {nullptr, "null (default -> Roboto fallback)"},
    {"ibm-plex-mono", "IBM Plex Mono"},
    {"public-sans", "Public Sans"},
}};

int CyclingFontIndex(double time_seconds, double interval_seconds);
std::string ResolveFontFamily(const FontChoice &choice);

} // namespace text
