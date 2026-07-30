#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkRefCnt.h"

class SkiaFontManager final {
public:
  static SkiaFontManager &Instance();

  bool RegisterFont(const std::string &font_id, const std::uint8_t *bytes,
                    std::size_t byte_count);
  sk_sp<SkFontMgr> FontManager() const;
  std::string FamilyNameForId(const std::string &font_id) const;

  SkiaFontManager(const SkiaFontManager &) = delete;
  SkiaFontManager &operator=(const SkiaFontManager &) = delete;

private:
  struct LoadedFont {
    std::string id;
    std::string family_name;
    sk_sp<SkData> data;
  };

  SkiaFontManager();
  ~SkiaFontManager();

  bool RebuildFontManager();

  sk_sp<SkData> default_font_data_;
  sk_sp<SkFontMgr> font_manager_;
  std::vector<LoadedFont> loaded_fonts_;
};
