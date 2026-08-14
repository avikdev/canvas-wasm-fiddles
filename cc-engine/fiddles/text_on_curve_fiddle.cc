#include "fiddles/text_on_curve_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "geometry/path_text_deformer.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/utils/SkParsePath.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "text/skia_font_manager.h"

namespace {

constexpr SkColor kCanvasColor = 0xffffe8f3;
constexpr SkColor kGuideColor = 0xffff0a78;
constexpr SkColor kWhiteTextColor = 0xffffffff;
constexpr SkColor kBlackTextColor = SK_ColorBLACK;
constexpr SkColor kSvgTextMarkerColor = 0xff010203;
constexpr std::string_view kSvgTextMarkerAttribute = " fill=\"#010203\"";
constexpr float kViewBoxSize = 100.0F;
constexpr float kGuidePaddingRatio = 0.08F;
constexpr float kAnimationEmsPerSecond = 1.8F;

std::optional<float> ParseFloat(const std::string &value) {
  char *end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0' || !std::isfinite(parsed)) {
    return std::nullopt;
  }
  return parsed;
}

std::string FontFamilyForKey(const std::string &key) {
  if (key == "IBM Plex Mono") {
    return SkiaFontManager::Instance().FamilyNameForId("ibm-plex-mono");
  }
  if (key == "Public Sans") {
    return SkiaFontManager::Instance().FamilyNameForId("public-sans");
  }
  return {};
}

std::string GroupSvgTextPaths(std::string svg, SkColor text_color) {
  std::vector<std::string> text_paths;
  std::size_t search_from = 0;
  while (true) {
    const std::size_t marker = svg.find(kSvgTextMarkerAttribute, search_from);
    if (marker == std::string::npos)
      break;
    const std::size_t path_start = svg.rfind("<path", marker);
    const std::size_t path_end = svg.find('>', marker);
    if (path_start == std::string::npos || path_end == std::string::npos ||
        path_start > marker) {
      search_from = marker + kSvgTextMarkerAttribute.size();
      continue;
    }
    std::string path = svg.substr(path_start, path_end - path_start + 1);
    const std::size_t local_marker = path.find(kSvgTextMarkerAttribute);
    path.erase(local_marker, kSvgTextMarkerAttribute.size());
    text_paths.push_back(std::move(path));
    svg.erase(path_start, path_end - path_start + 1);
    search_from = path_start;
  }
  if (text_paths.empty())
    return svg;

  const char *fill = text_color == kBlackTextColor ? "#000000" : "#ffffff";
  std::string group = "<g id=\"text-on-curve-text\" fill=\"";
  group += fill;
  group += "\">\n";
  for (const std::string &path : text_paths) {
    group += path;
    group += '\n';
  }
  group += "</g>\n";
  const std::size_t svg_end = svg.rfind("</svg>");
  svg.insert(svg_end == std::string::npos ? svg.size() : svg_end, group);
  return svg;
}

} // namespace

TextOnCurveFiddle::TextOnCurveFiddle() = default;
TextOnCurveFiddle::~TextOnCurveFiddle() = default;

std::vector<FiddleWidget> TextOnCurveFiddle::Widgets() const {
  return {
      {"text", "Text", "para", text_},
      {"path-string", "Path as SVG string, using viewbox [0 0 100 100]", "para",
       path_string_},
      {"font-face",
       "Font Face",
       "option",
       font_face_,
       {"default (Roboto)", "IBM Plex Mono", "Public Sans"}},
      {"use-fixed-font-size", "Use Fixed Font Size", "bool",
       use_fixed_font_size_ ? "true" : "false"},
      {"font-size",
       "Font Size (when fixed font-size is enabled)",
       "range",
       std::to_string(font_size_),
       {},
       10.0,
       32.0,
       1.0},
      {"speed",
       "Animation speed",
       "range",
       std::to_string(animation_speed_),
       {},
       0.0,
       50.0,
       0.5},
      {"black-text", "Use Black text", "bool", black_text_ ? "true" : "false"},
      {"protect-sharp-turns", "Protect text at sharp turns", "bool",
       protect_sharp_turns_ ? "true" : "false"},
      {"sharp-turn-safety",
       "Sharp-turn inner radius safety",
       "range",
       std::to_string(sharp_turn_safety_),
       {},
       0.55,
       0.95,
       0.05},
      {"subdivision-tolerance",
       "Curve subdivision tolerance",
       "range",
       std::to_string(subdivision_tolerance_),
       {},
       0.1,
       1.0,
       0.05},
  };
}

bool TextOnCurveFiddle::SetInput(const std::string &key,
                                 const std::string &value) {
  if (key == "text") {
    if (text::NormalizeSingleLineText(value).empty())
      return false;
    text_ = value;
    text_dirty_ = true;
    return true;
  }
  if (key == "path-string") {
    const std::optional<SkPath> parsed =
        SkParsePath::FromSVGString(value.c_str());
    if (!parsed.has_value() || parsed->isEmpty() || !parsed->isFinite()) {
      return false;
    }
    path_string_ = SkParsePath::ToSVGString(*parsed).c_str();
    guide_dirty_ = true;
    return true;
  }
  if (key == "font-face") {
    if (value != "default (Roboto)" && value != "IBM Plex Mono" &&
        value != "Public Sans") {
      return false;
    }
    font_face_ = value;
    text_dirty_ = true;
    return true;
  }
  if (key == "use-fixed-font-size") {
    use_fixed_font_size_ = value == "true";
    text_dirty_ = true;
    return true;
  }
  if (key == "black-text") {
    black_text_ = value == "true";
    return true;
  }
  if (key == "protect-sharp-turns") {
    protect_sharp_turns_ = value == "true";
    return true;
  }
  const std::optional<float> parsed = ParseFloat(value);
  if (!parsed.has_value())
    return false;
  if (key == "font-size") {
    font_size_ = std::clamp(*parsed, 10.0F, 32.0F);
    text_dirty_ = true;
    return true;
  }
  if (key == "speed") {
    animation_speed_ = std::clamp(*parsed, 0.0F, 50.0F);
    return true;
  }
  if (key == "sharp-turn-safety") {
    sharp_turn_safety_ = std::clamp(*parsed, 0.55F, 0.95F);
    return true;
  }
  if (key == "subdivision-tolerance") {
    subdivision_tolerance_ = std::clamp(*parsed, 0.1F, 1.0F);
    return true;
  }
  return false;
}

bool TextOnCurveFiddle::EnsureResources() {
  if (webgl_ != nullptr && font_collection_ != nullptr)
    return true;
  if (initialization_attempted_)
    return false;
  initialization_attempted_ = true;

  SkiaFontManager &shared_fonts = SkiaFontManager::Instance();
  const sk_sp<SkFontMgr> font_manager = shared_fonts.FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    return false;
  }
  font_collection_ = sk_make_sp<skia::textlayout::FontCollection>();
  font_collection_->setDefaultFontManager(font_manager, "Roboto");

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource()))
    return false;
  webgl_ = std::move(webgl);
  return true;
}

bool TextOnCurveFiddle::RebuildGuide(int width, int height) {
  const std::optional<SkPath> parsed =
      SkParsePath::FromSVGString(path_string_.c_str());
  if (!parsed.has_value() || parsed->isEmpty())
    return false;
  const float shortest = static_cast<float>(std::min(width, height));
  const float available = shortest * (1.0F - kGuidePaddingRatio * 2.0F);
  const float scale = available / kViewBoxSize;
  const float left = (static_cast<float>(width) - available) * 0.5F;
  const float top = (static_cast<float>(height) - available) * 0.5F;
  parsed->transform(SkMatrix::ScaleTranslate(scale, scale, left, top),
                    &guide_path_);
  deformer_ = std::make_unique<geometry::PathTextDeformer>(guide_path_);
  if (!deformer_->valid())
    return false;
  guide_dirty_ = false;
  return true;
}

bool TextOnCurveFiddle::RebuildText(int width, int height) {
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  if (use_fixed_font_size_) {
    effective_font_size_ = font_size_ * device_scale;
  } else {
    const float logical_shortest =
        static_cast<float>(std::min(Width(), Height()));
    const float logical_size =
        std::clamp(logical_shortest * 0.055F, 14.0F, 32.0F);
    effective_font_size_ = logical_size * device_scale;
  }
  std::string repeated_unit = text::NormalizeSingleLineText(text_);
  if (repeated_unit.empty())
    return false;
  repeated_unit.push_back(' ');
  if (!text::ShapeTextLine(repeated_unit, FontFamilyForKey(font_face_),
                           effective_font_size_, font_collection_,
                           &shaped_line_)) {
    return false;
  }
  text_dirty_ = false;
  cached_width_ = width;
  cached_height_ = height;
  return true;
}

void TextOnCurveFiddle::Render(double time_seconds) {
  if (!EnsureResources())
    return;
  const int width = PixelWidth();
  const int height = PixelHeight();
  if (width != cached_width_ || height != cached_height_) {
    guide_dirty_ = true;
    text_dirty_ = true;
  }
  if ((guide_dirty_ && !RebuildGuide(width, height)) ||
      (text_dirty_ && !RebuildText(width, height))) {
    return;
  }
  if (has_previous_frame_time_) {
    const double elapsed =
        std::clamp(time_seconds - previous_frame_time_seconds_, 0.0, 0.1);
    motion_offset_ += static_cast<float>(elapsed) * effective_font_size_ *
                      kAnimationEmsPerSecond * (animation_speed_ / 50.0F);
    motion_offset_ = std::fmod(motion_offset_, shaped_line_.advance);
  }
  previous_frame_time_seconds_ = time_seconds;
  has_previous_frame_time_ = true;
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr)
    return;
  DrawFrame(surface->getCanvas(), width, height);
  if (!webgl_->FlushAndPresent().success) {
    std::cerr << "[cc-engine/stderr] Text On Curve could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

void TextOnCurveFiddle::DrawFrame(SkCanvas *canvas, int width, int /*height*/) {
  canvas->clear(kCanvasColor);
  if (deformer_ == nullptr || !deformer_->valid() || !shaped_line_.valid()) {
    return;
  }

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeWidth(shaped_line_.symmetric_half_height * 2.0F * 1.20F);
  paint.setColor(kGuideColor);
  canvas->drawPath(guide_path_, paint);

  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const std::array<SkScalar, 2> dots = {1.2F * device_scale,
                                        4.2F * device_scale};
  paint.setStrokeWidth(std::max(1.0F, 1.2F * device_scale));
  paint.setColor(0x4d000000);
  paint.setPathEffect(SkDashPathEffect::Make(dots, 0.0F));
  canvas->drawPath(guide_path_, paint);
  paint.setPathEffect(nullptr);

  geometry::PathTextDeformOptions options;
  options.minimum_segment_length = std::max(0.20F, device_scale * 0.20F);
  options.maximum_segment_length =
      std::max(options.minimum_segment_length, effective_font_size_ * 0.09F);
  options.flatness_tolerance = subdivision_tolerance_ * device_scale;
  options.curvature_probe =
      std::max(device_scale, effective_font_size_ * 0.22F);
  options.protect_sharp_turns = protect_sharp_turns_;
  options.inversion_safety = sharp_turn_safety_;

  const float line_advance = shaped_line_.advance;
  const float motion = std::fmod(motion_offset_, line_advance);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(IsExportingSvg()
                     ? kSvgTextMarkerColor
                     : (black_text_ ? kBlackTextColor : kWhiteTextColor));
  const float guide_length = deformer_->length();
  for (float repeat_start = -motion - line_advance;
       repeat_start < guide_length + line_advance;
       repeat_start += line_advance) {
    for (const text::ShapedGlyph &glyph : shaped_line_.glyphs) {
      const SkRect bounds = glyph.outline.getBounds();
      const float left = repeat_start + bounds.left();
      const float right = repeat_start + bounds.right();
      if (right <= 0.0F || left >= guide_length)
        continue;
      const SkPath deformed =
          deformer_->Deform(glyph.outline, repeat_start, options);
      if (!deformed.isEmpty())
        canvas->drawPath(deformed, paint);
    }
  }
}

std::string TextOnCurveFiddle::PostProcessSvg(std::string svg) const {
  return GroupSvgTextPaths(std::move(svg),
                           black_text_ ? kBlackTextColor : kWhiteTextColor);
}
