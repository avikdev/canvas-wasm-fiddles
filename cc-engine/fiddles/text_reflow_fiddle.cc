#include "fiddles/text_reflow_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradientShader.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "text/font_cycle.h"
#include "text/skia_font_manager.h"
#include "utils/random_permutation.h"

namespace {

using skia::textlayout::ParagraphBuilder;
using skia::textlayout::ParagraphStyle;
using skia::textlayout::StrutStyle;
using skia::textlayout::TextAlign;
using skia::textlayout::TextDirection;
using skia::textlayout::TextStyle;

constexpr double kPanelWidthCycleSeconds = 10.0;
constexpr double kContentCycleSeconds = 5.0;
constexpr float kPhoneWidthThreshold = 600.0F;
constexpr int kAlignmentCount = 9;
constexpr int kFontCount = static_cast<int>(text::kFontChoices.size());

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

TextReflowFiddle::TextReflowFiddle()
    : alignment_sequence_(
          utils::CreateRandomGridTraversal(3, 3, CreateAlignmentSeed())) {}

TextReflowFiddle::~TextReflowFiddle() = default;

std::vector<FiddleWidget> TextReflowFiddle::Widgets() const {
  return {
      {"text", "Paragraph text", "para", text_},
      {"height-multiplier",
       "Relative line height",
       "range",
       "1",
       {},
       0.1,
       2.5,
       0.1},
      {"letter-spacing",
       "Letter Spacing (em)",
       "range",
       "0",
       {},
       -0.1,
       0.25,
       0.01},
      {"word-spacing", "Word Spacing (em)", "range", "0", {}, -0.5, 1.0, 0.05},
      {"baseline-shift",
       "Baseline Shift (em)",
       "range",
       "0",
       {},
       -0.5,
       0.5,
       0.05}};
}

bool TextReflowFiddle::SetInput(const std::string &key,
                                const std::string &value) {
  if (key == "text") {
    text_ = value;
  } else {
    char *end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0')
      return false;
    if (key == "height-multiplier") {
      height_multiplier_ = std::clamp(parsed, 0.1F, 2.5F);
    } else if (key == "letter-spacing") {
      letter_spacing_ = std::clamp(parsed, -0.1F, 0.25F);
    } else if (key == "word-spacing") {
      word_spacing_ = std::clamp(parsed, -0.5F, 1.0F);
    } else if (key == "baseline-shift") {
      baseline_shift_ = std::clamp(parsed, -0.5F, 0.5F);
    } else {
      return false;
    }
  }
  paragraph_font_size_ = 0.0F;
  paragraph_gradient_width_ = 0.0F;
  return true;
}

bool TextReflowFiddle::EnsureResources() {
  if (webgl_ != nullptr && font_collection_ != nullptr &&
      label_typeface_ != nullptr && bold_label_typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }

  SkiaFontManager &shared_fonts = SkiaFontManager::Instance();
  sk_sp<SkFontMgr> font_manager = shared_fonts.FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Text Reflow could not access the shared "
                 "font manager."
              << std::endl;
    return false;
  }

  label_typeface_ =
      font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  if (label_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Text Reflow could not resolve its "
                 "Roboto fallback typeface."
              << std::endl;
    return false;
  }
  bold_label_typeface_ =
      font_manager->matchFamilyStyle("Roboto", SkFontStyle::Bold());
  if (bold_label_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Text Reflow could not resolve its "
                 "Roboto bold label typeface."
              << std::endl;
    return false;
  }

  font_collection_ = sk_make_sp<skia::textlayout::FontCollection>();
  font_collection_->setDefaultFontManager(font_manager, "Roboto");

  webgl_ = std::move(webgl);
  std::cout << "[cc-engine/stdout] Text Reflow paragraph resources ready: "
               "families=Roboto, "
            << shared_fonts.FamilyNameForId("ibm-plex-mono") << ", "
            << shared_fonts.FamilyNameForId("public-sans") << "." << std::endl;
  return true;
}

bool TextReflowFiddle::RebuildParagraphs(float font_size,
                                         float gradient_width) {
  sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
  if (unicode == nullptr) {
    return false;
  }

  constexpr SkColor kGradientColors[] = {
      SkColorSetRGB(0, 123, 255),
      SkColorSetRGB(190, 0, 104),
  };
  const SkPoint gradient_points[] = {
      SkPoint::Make(gradient_width * 0.18F, 0.0F),
      SkPoint::Make(gradient_width * 0.82F, 0.0F),
  };
  SkPaint gradient_paint;
  gradient_paint.setShader(SkGradientShader::MakeLinear(
      gradient_points, kGradientColors, nullptr, std::size(kGradientColors),
      SkTileMode::kClamp));

  for (int font_index = 0; font_index < kFontCount; ++font_index) {
    const text::FontChoice &font_choice = text::kFontChoices[font_index];
    const std::string family_name = text::ResolveFontFamily(font_choice);
    const auto configure_style = [&](TextStyle *style) {
      if (family_name.empty()) {
        style->setFontFamilies({});
      } else {
        style->setFontFamilies({SkString(family_name.c_str())});
      }
      style->setFontSize(font_size);
      style->setHeight(height_multiplier_);
      style->setHeightOverride(true);
      style->setLetterSpacing(letter_spacing_ * font_size);
      // SkParagraph consumes both values as absolute scalars. Store the UI
      // values as em ratios so their effect follows the responsive font size.
      style->setWordSpacing(word_spacing_ * font_size);
      style->setBaselineShift(baseline_shift_ * font_size);
    };

    TextStyle body_style;
    configure_style(&body_style);
    body_style.setForegroundPaint(gradient_paint);

    for (int alignment_index = 0; alignment_index < kAlignmentCount;
         ++alignment_index) {
      ParagraphStyle paragraph_style;
      paragraph_style.setTextDirection(TextDirection::kLtr);
      paragraph_style.setTextAlign(kAlignments[alignment_index].horizontal);
      paragraph_style.setFakeMissingFontStyles(true);

      // A uniform baseline shift otherwise participates in the run's own line
      // metrics, which makes the baseline move with the glyphs and masks the
      // visual shift. A forced strut supplies stable line metrics while the
      // TextStyle baseline shift is applied to glyph placement.
      StrutStyle strut_style;
      if (family_name.empty()) {
        strut_style.setFontFamilies({});
      } else {
        strut_style.setFontFamilies({SkString(family_name.c_str())});
      }
      strut_style.setFontSize(font_size);
      strut_style.setHeight(height_multiplier_);
      strut_style.setHeightOverride(true);
      strut_style.setStrutEnabled(true);
      strut_style.setForceStrutHeight(true);
      paragraph_style.setStrutStyle(std::move(strut_style));

      auto builder =
          ParagraphBuilder::make(paragraph_style, font_collection_, unicode);
      if (builder == nullptr) {
        std::cerr << "[cc-engine/stderr] Text Reflow could not create a Skia "
                     "ParagraphBuilder."
                  << std::endl;
        return false;
      }

      builder->pushStyle(body_style);
      builder->addText(text_.data(), text_.size());
      builder->pop();

      paragraphs_[font_index][alignment_index] = builder->Build();
      if (paragraphs_[font_index][alignment_index] == nullptr) {
        return false;
      }
    }
  }

  paragraph_font_size_ = font_size;
  paragraph_gradient_width_ = gradient_width;
  return true;
}

void TextReflowFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }

  const int width = PixelWidth();
  const int height = PixelHeight();
  if (!UpdateState(time_seconds, width, height)) {
    return;
  }
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Text Reflow could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

bool TextReflowFiddle::UpdateState(double time_seconds, int width, int height) {
  const double cycle_position =
      std::fmod(time_seconds, kPanelWidthCycleSeconds) /
      kPanelWidthCycleSeconds;
  const std::size_t sequence_position =
      static_cast<std::size_t>(time_seconds / kContentCycleSeconds) %
      alignment_sequence_.size();
  current_alignment_index_ = alignment_sequence_[sequence_position];
  current_font_index_ =
      text::CyclingFontIndex(time_seconds, kContentCycleSeconds);
  current_width_ratio_ =
      0.575F + 0.175F * std::cos(static_cast<float>(cycle_position * 2.0 *
                                                    std::numbers::pi));

  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  const float shortest = std::min(canvas_width, canvas_height);
  const float panel_width = canvas_width * current_width_ratio_;
  const float panel_left = (canvas_width - panel_width) * 0.5F;
  const float panel_top = canvas_height * 0.15F;
  const float panel_bottom = canvas_height * 0.85F;
  const SkRect panel = SkRect::MakeLTRB(panel_left, panel_top,
                                        panel_left + panel_width, panel_bottom);

  const float panel_padding = std::clamp(panel_width * 0.045F, 24.0F, 56.0F);
  const SkRect text_bounds = SkRect::MakeLTRB(
      panel.left() + panel_padding, panel.top() + panel_padding,
      panel.right() - panel_padding, panel.bottom() - panel_padding);
  const float paragraph_font_size =
      Width() <= kPhoneWidthThreshold
          ? std::max(1.0F, std::round(canvas_height * 0.04F))
          : std::clamp(shortest * 0.100F, 50.0F, 92.0F);

  if (paragraphs_[0][0] == nullptr ||
      std::abs(paragraph_font_size_ - paragraph_font_size) > 0.1F ||
      std::abs(paragraph_gradient_width_ - canvas_width) > 0.1F) {
    if (!RebuildParagraphs(paragraph_font_size, canvas_width)) {
      return false;
    }
  }
  skia::textlayout::Paragraph *paragraph =
      paragraphs_[current_font_index_][current_alignment_index_].get();
  paragraph->layout(text_bounds.width());
  return true;
}

void TextReflowFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  const int alignment_index = current_alignment_index_;
  const int font_index = current_font_index_;
  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  const float shortest = std::min(canvas_width, canvas_height);
  const float panel_width = canvas_width * current_width_ratio_;
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
  skia::textlayout::Paragraph *paragraph =
      paragraphs_[font_index][alignment_index].get();

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
  SkFont bold_label_font(bold_label_typeface_, label_font_size);
  bold_label_font.setEdging(SkFont::Edging::kAntiAlias);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);

  constexpr char kIconMeasureText[] = "000000";
  const float measured_icon_size = label_font.measureText(
      kIconMeasureText, sizeof(kIconMeasureText) - 1, SkTextEncoding::kUTF8);
  const float legend_bottom_margin = std::max(4.0F, canvas_height * 0.015F);
  const float icon_size = std::min(
      measured_icon_size, std::max(24.0F, canvas_height - separator_y -
                                              legend_bottom_margin * 2.0F));
  constexpr char kAlignmentLabel[] = "Align:";
  const float alignment_label_width = label_font.measureText(
      kAlignmentLabel, sizeof(kAlignmentLabel) - 1, SkTextEncoding::kUTF8);
  const float alignment_gap = label_font_size * 0.45F;
  const float alignment_label_x = canvas_width * 0.05F;
  const SkRect alignment_icon = SkRect::MakeXYWH(
      alignment_label_x + alignment_label_width + alignment_gap,
      canvas_height - legend_bottom_margin - icon_size, icon_size, icon_size);
  const float legend_baseline =
      alignment_icon.top() + (icon_size + label_font_size) * 0.5F;
  canvas->drawSimpleText(kAlignmentLabel, sizeof(kAlignmentLabel) - 1,
                         SkTextEncoding::kUTF8, alignment_label_x,
                         legend_baseline, label_font, paint);
  DrawAlignmentIcon(canvas, alignment_icon, alignment_index);

  const text::FontChoice &font_choice = text::kFontChoices[font_index];
  const std::string family_name = text::ResolveFontFamily(font_choice);
  constexpr char kFontPrefix[] = "Font: ";
  const std::string font_value =
      font_choice.font_id == nullptr ? "default (Roboto)"
      : family_name.empty()
          ? std::string(font_choice.display_name) + " (unavailable -> fallback)"
          : std::string(font_choice.display_name);
  const float font_prefix_width = label_font.measureText(
      kFontPrefix, sizeof(kFontPrefix) - 1, SkTextEncoding::kUTF8);
  const float font_value_width = bold_label_font.measureText(
      font_value.data(), font_value.size(), SkTextEncoding::kUTF8);
  const float font_label_x =
      canvas_width * 0.95F - font_prefix_width - font_value_width;
  canvas->drawSimpleText(kFontPrefix, sizeof(kFontPrefix) - 1,
                         SkTextEncoding::kUTF8, font_label_x, legend_baseline,
                         label_font, paint);
  canvas->drawSimpleText(font_value.data(), font_value.size(),
                         SkTextEncoding::kUTF8,
                         font_label_x + font_prefix_width, legend_baseline,
                         bold_label_font, paint);
}
