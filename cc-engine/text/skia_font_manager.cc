#include "text/skia_font_manager.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <utility>

#include "include/core/SkSpan.h"
#include "include/core/SkString.h"
#include "include/ports/SkFontMgr_data.h"

extern unsigned char kSkiaDefaultFont[];
extern unsigned int kSkiaDefaultFont_len;

SkiaFontManager &SkiaFontManager::Instance() {
  static SkiaFontManager instance;
  return instance;
}

SkiaFontManager::SkiaFontManager()
    : default_font_data_(
          SkData::MakeWithoutCopy(kSkiaDefaultFont, kSkiaDefaultFont_len)) {
  if (!RebuildFontManager()) {
    std::cerr << "[cc-engine/stderr] Could not initialize the shared Skia "
                 "font manager."
              << std::endl;
  }
}

SkiaFontManager::~SkiaFontManager() = default;

bool SkiaFontManager::RegisterFont(const std::string &font_id,
                                   const std::uint8_t *bytes,
                                   std::size_t byte_count) {
  if (font_id.empty() || bytes == nullptr || byte_count == 0) {
    return false;
  }

  sk_sp<SkData> data = SkData::MakeWithCopy(bytes, byte_count);
  std::array<sk_sp<SkData>, 1> candidate_data = {data};
  sk_sp<SkFontMgr> candidate_manager =
      SkFontMgr_New_Custom_Data(SkSpan(candidate_data));
  if (candidate_manager == nullptr || candidate_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Skia rejected external font: " << font_id
              << std::endl;
    return false;
  }

  SkString family_name;
  candidate_manager->getFamilyName(0, &family_name);
  if (family_name.isEmpty()) {
    return false;
  }

  const std::vector<LoadedFont> previous_fonts = loaded_fonts_;
  auto existing = std::find_if(
      loaded_fonts_.begin(), loaded_fonts_.end(),
      [&font_id](const LoadedFont &font) { return font.id == font_id; });
  LoadedFont loaded = {font_id, family_name.c_str(), std::move(data)};
  if (existing == loaded_fonts_.end()) {
    loaded_fonts_.push_back(std::move(loaded));
  } else {
    *existing = std::move(loaded);
  }

  if (!RebuildFontManager()) {
    loaded_fonts_ = previous_fonts;
    RebuildFontManager();
    return false;
  }

  std::cout << "[cc-engine/stdout] Registered external font: id=" << font_id
            << ", family=" << family_name.c_str() << ", bytes=" << byte_count
            << "." << std::endl;
  return true;
}

sk_sp<SkFontMgr> SkiaFontManager::FontManager() const { return font_manager_; }

std::string SkiaFontManager::FamilyNameForId(const std::string &font_id) const {
  const auto font = std::find_if(
      loaded_fonts_.begin(), loaded_fonts_.end(),
      [&font_id](const LoadedFont &loaded) { return loaded.id == font_id; });
  return font == loaded_fonts_.end() ? std::string() : font->family_name;
}

bool SkiaFontManager::RebuildFontManager() {
  std::vector<sk_sp<SkData>> font_data;
  font_data.reserve(loaded_fonts_.size() + 1);
  font_data.push_back(default_font_data_);
  for (const LoadedFont &font : loaded_fonts_) {
    font_data.push_back(font.data);
  }

  sk_sp<SkFontMgr> manager = SkFontMgr_New_Custom_Data(SkSpan(font_data));
  if (manager == nullptr ||
      manager->countFamilies() < static_cast<int>(font_data.size())) {
    return false;
  }
  font_manager_ = std::move(manager);
  return true;
}
