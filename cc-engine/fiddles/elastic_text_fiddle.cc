#include "fiddles/elastic_text_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <random>
#include <string>

#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "text/skia_font_manager.h"
#include "utils/random_permutation.h"

namespace {

using skia::textlayout::ParagraphBuilder;
using skia::textlayout::ParagraphStyle;
using skia::textlayout::TextAlign;
using skia::textlayout::TextDirection;
using skia::textlayout::TextStyle;

constexpr double kWidthCycleSeconds = 5.0;
constexpr int kAlignmentCount = 9;
constexpr int kFontCount = 3;

struct Alignment {
  TextAlign horizontal;
};

constexpr std::array<Alignment, kAlignmentCount> kAlignments = {{
    {TextAlign::kLeft},
    {TextAlign::kCenter},
    {TextAlign::kRight},
    {TextAlign::kLeft},
    {TextAlign::kCenter},
    {TextAlign::kRight},
    {TextAlign::kLeft},
    {TextAlign::kCenter},
    {TextAlign::kRight},
}};

struct FontChoice {
  const char *font_id;
  const char *display_name;
};

constexpr std::array<FontChoice, kFontCount> kFontChoices = {{
    {nullptr, "null (default -> Roboto fallback)"},
    {"ibm-plex-mono", "IBM Plex Mono"},
    {"public-sans", "Public Sans"},
}};

float VerticalTextOffset(int alignment_index, float available_height,
                         float paragraph_height) {
  const float remaining = std::max(0.0F, available_height - paragraph_height);
  switch (alignment_index / 3) {
  case 1:
    return remaining * 0.5F;
  case 2:
    return remaining;
  default:
    return 0.0F;
  }
}

std::uint32_t CreateAlignmentSeed() {
  std::random_device random;
  return random();
}

void DrawAlignmentIcon(SkCanvas *canvas, const SkRect &bounds,
                       int alignment_index) {
  constexpr int kGridSize = 3;
  const float cell_size = bounds.width() / static_cast<float>(kGridSize);
  const int active_row = alignment_index / kGridSize;
  const int active_column = alignment_index % kGridSize;

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRect(SkRect::MakeXYWH(bounds.left() + active_column * cell_size,
                                    bounds.top() + active_row * cell_size,
                                    cell_size, cell_size),
                   paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1.0F);
  canvas->drawRect(bounds, paint);
  for (int divider = 1; divider < kGridSize; ++divider) {
    const float offset = static_cast<float>(divider) * cell_size;
    canvas->drawLine(bounds.left() + offset, bounds.top(),
                     bounds.left() + offset, bounds.bottom(), paint);
    canvas->drawLine(bounds.left(), bounds.top() + offset, bounds.right(),
                     bounds.top() + offset, paint);
  }
}

} // namespace

ElasticTextFiddle::ElasticTextFiddle()
    : alignment_sequence_(
          utils::CreateRandomGridTraversal(3, 3, CreateAlignmentSeed())) {}

ElasticTextFiddle::~ElasticTextFiddle() = default;

bool ElasticTextFiddle::UsesWebGl() const { return true; }

bool ElasticTextFiddle::EnsureResources() {
  if (webgl_ != nullptr && font_collection_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(Canvas())) {
    return false;
  }

  SkiaFontManager &shared_fonts = SkiaFontManager::Instance();
  sk_sp<SkFontMgr> font_manager = shared_fonts.FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Elastic text could not access the shared "
                 "font manager."
              << std::endl;
    return false;
  }

  label_typeface_ =
      font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  if (label_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Elastic text could not resolve its "
                 "Roboto fallback typeface."
              << std::endl;
    return false;
  }

  font_collection_ = sk_make_sp<skia::textlayout::FontCollection>();
  font_collection_->setDefaultFontManager(font_manager, "Roboto");

  webgl_ = std::move(webgl);
  std::cout << "[cc-engine/stdout] Elastic text paragraph resources ready: "
               "families=Roboto, "
            << shared_fonts.FamilyNameForId("ibm-plex-mono") << ", "
            << shared_fonts.FamilyNameForId("public-sans") << "." << std::endl;
  return true;
}

bool ElasticTextFiddle::RebuildParagraphs(float font_size) {
  SkiaFontManager &shared_fonts = SkiaFontManager::Instance();
  sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
  if (unicode == nullptr) {
    return false;
  }

  for (int font_index = 0; font_index < kFontCount; ++font_index) {
    TextStyle body_style;
    const char *font_id = kFontChoices[font_index].font_id;
    const std::string family_name = font_id == nullptr
                                        ? std::string()
                                        : shared_fonts.FamilyNameForId(font_id);
    if (family_name.empty()) {
      body_style.setFontFamilies({});
    } else {
      body_style.setFontFamilies({SkString(family_name.c_str())});
    }
    body_style.setFontSize(font_size);
    body_style.setColor(SkColorSetRGB(90, 24, 154));
    body_style.setHeight(1.18F);
    body_style.setHeightOverride(true);

    TextStyle warm_accent_style = body_style;
    warm_accent_style.setColor(SkColorSetRGB(165, 56, 96));
    warm_accent_style.setFontStyle(SkFontStyle::Bold());
    TextStyle teal_accent_style = body_style;
    teal_accent_style.setColor(SkColorSetRGB(10, 147, 150));
    teal_accent_style.setFontStyle(SkFontStyle::Bold());
    TextStyle green_accent_style = body_style;
    green_accent_style.setColor(SkColorSetRGB(82, 121, 111));
    green_accent_style.setFontStyle(SkFontStyle::Bold());

    for (int alignment_index = 0; alignment_index < kAlignmentCount;
         ++alignment_index) {
      ParagraphStyle paragraph_style;
      paragraph_style.setTextDirection(TextDirection::kLtr);
      paragraph_style.setTextAlign(kAlignments[alignment_index].horizontal);
      paragraph_style.setFakeMissingFontStyles(true);

      auto builder =
          ParagraphBuilder::make(paragraph_style, font_collection_, unicode);
      if (builder == nullptr) {
        std::cerr << "[cc-engine/stderr] Elastic text could not create a Skia "
                     "ParagraphBuilder."
                  << std::endl;
        return false;
      }

      builder->pushStyle(body_style);
      builder->addText("\"Ideas do not always ask for more ");
      builder->pushStyle(warm_accent_style);
      builder->addText("room");
      builder->pop();
      builder->addText(". They learn the ");
      builder->pushStyle(teal_accent_style);
      builder->addText("shape");
      builder->pop();
      builder->addText(" of the space, find a new ");
      builder->pushStyle(green_accent_style);
      builder->addText("line");
      builder->pop();
      builder->addText(", and keep their meaning as the edges move.\"");
      builder->pop();

      paragraphs_[font_index][alignment_index] = builder->Build();
      if (paragraphs_[font_index][alignment_index] == nullptr) {
        return false;
      }
    }
  }

  paragraph_font_size_ = font_size;
  return true;
}

void ElasticTextFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }

  const int width = Canvas()["width"].as<int>();
  const int height = Canvas()["height"].as<int>();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }

  const double cycle_position =
      std::fmod(time_seconds, kWidthCycleSeconds) / kWidthCycleSeconds;
  const std::size_t sequence_position =
      static_cast<std::size_t>(time_seconds / kWidthCycleSeconds) %
      alignment_sequence_.size();
  const int alignment_index = alignment_sequence_[sequence_position];
  const int font_index = static_cast<int>(sequence_position % kFontCount);
  const float width_ratio =
      0.575F + 0.175F * std::cos(static_cast<float>(cycle_position * 2.0 *
                                                    std::numbers::pi));

  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  const float shortest = std::min(canvas_width, canvas_height);
  const float panel_width = canvas_width * width_ratio;
  const float panel_left = (canvas_width - panel_width) * 0.5F;
  const float panel_top = canvas_height * 0.15F;
  const float panel_bottom = canvas_height * 0.85F;
  const SkRect panel = SkRect::MakeLTRB(panel_left, panel_top,
                                        panel_left + panel_width, panel_bottom);

  const float panel_padding = std::clamp(panel_width * 0.045F, 24.0F, 56.0F);
  const float label_font_size = std::clamp(shortest * 0.030F, 20.0F, 28.0F);
  const SkRect text_bounds = SkRect::MakeLTRB(
      panel.left() + panel_padding, panel.top() + panel_padding,
      panel.right() - panel_padding, panel.bottom() - panel_padding);
  const float paragraph_font_size = std::clamp(shortest * 0.050F, 25.0F, 46.0F);

  if (paragraphs_[0][0] == nullptr ||
      std::abs(paragraph_font_size_ - paragraph_font_size) > 0.1F) {
    if (!RebuildParagraphs(paragraph_font_size)) {
      return;
    }
  }
  skia::textlayout::Paragraph *paragraph =
      paragraphs_[font_index][alignment_index].get();
  paragraph->layout(text_bounds.width());

  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(SkColorSetRGB(204, 213, 174));

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SkColorSetRGB(254, 250, 224));
  canvas->drawRoundRect(panel, 8.0F, 8.0F, paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(2.0F);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRoundRect(panel, 8.0F, 8.0F, paint);

  const float paragraph_y =
      text_bounds.top() + VerticalTextOffset(alignment_index,
                                             text_bounds.height(),
                                             paragraph->getHeight());
  canvas->save();
  canvas->clipRect(text_bounds);
  paragraph->paint(canvas, text_bounds.left(), paragraph_y);
  canvas->restore();

  const float legend_height = canvas_height - panel.bottom();
  const float separator_y = panel.bottom() + legend_height * 0.18F;
  paint.setStrokeWidth(1.0F);
  paint.setColor(SK_ColorBLACK);
  canvas->drawLine(canvas_width * 0.05F, separator_y, canvas_width * 0.95F,
                   separator_y, paint);

  SkFont label_font(label_typeface_, label_font_size);
  label_font.setEdging(SkFont::Edging::kAntiAlias);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);

  constexpr char kIconMeasureText[] = "000000";
  const float measured_icon_size = label_font.measureText(
      kIconMeasureText, sizeof(kIconMeasureText) - 1, SkTextEncoding::kUTF8);
  const float legend_bottom_margin = std::max(4.0F, canvas_height * 0.015F);
  const float icon_size = std::min(
      measured_icon_size, std::max(24.0F, canvas_height - separator_y -
                                              legend_bottom_margin * 2.0F));
  const SkRect alignment_icon = SkRect::MakeXYWH(
      canvas_width * 0.05F, canvas_height - legend_bottom_margin - icon_size,
      icon_size, icon_size);
  DrawAlignmentIcon(canvas, alignment_icon, alignment_index);

  const FontChoice &font_choice = kFontChoices[font_index];
  const std::string family_name =
      font_choice.font_id == nullptr
          ? std::string()
          : SkiaFontManager::Instance().FamilyNameForId(font_choice.font_id);
  const std::string font_label =
      family_name.empty() && font_choice.font_id != nullptr
          ? "Font: " + std::string(font_choice.display_name) +
                " (unavailable -> fallback)"
          : "Font: " + std::string(font_choice.display_name);
  const float font_label_width = label_font.measureText(
      font_label.data(), font_label.size(), SkTextEncoding::kUTF8);
  const float font_label_x = canvas_width * 0.95F - font_label_width;
  const float font_label_y =
      alignment_icon.top() + (icon_size + label_font_size) * 0.5F;
  canvas->drawSimpleText(font_label.data(), font_label.size(),
                         SkTextEncoding::kUTF8, font_label_x, font_label_y,
                         label_font, paint);

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Elastic text could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}
