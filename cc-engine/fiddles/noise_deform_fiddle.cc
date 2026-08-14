#include "fiddles/noise_deform_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

#include "geometry/noise_deform.h"
#include "graphics/canvas_widgets.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkRect.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/pathops/SkPathOps.h"
#include "text/skia_font_manager.h"
#include "utils/perlin_noise.h"

namespace {

constexpr float kConfigurationSeconds = 10.0F;
constexpr float kContourSampleSpacing = 1.8F;
constexpr SkColor kCanvasColor = 0xfff7f9fc;
constexpr SkColor kWaterColor = 0x2424a8e8;
constexpr SkColor kWaterLineColor = 0x7020a6dd;

struct NoiseConfiguration {
  std::string_view word;
  std::array<SkColor, 4> palette;
  int palette_size;
};

constexpr std::array<NoiseConfiguration, 3> kConfigurations = {{
    {"MORNING", {0xff08c7e8, 0xff147df5, 0xff13b8a6, 0xff36d6e7}, 4},
    {"DAY", {0xffffb11b, 0xffd97706, 0xffdc3f24, 0xffff7b20}, 3},
    {"NIGHT", {0xffff5ca8, 0xffef2f5f, 0xffd946ef, 0xffff397d}, 4},
}};

SkRect JoinBounds(const std::vector<SkPath> &paths) {
  SkRect bounds = SkRect::MakeEmpty();
  bool has_bounds = false;
  for (const SkPath &path : paths) {
    if (path.isEmpty()) {
      continue;
    }
    if (!has_bounds) {
      bounds = path.computeTightBounds();
      has_bounds = true;
    } else {
      bounds.join(path.computeTightBounds());
    }
  }
  return has_bounds ? bounds : SkRect::MakeEmpty();
}

SkPath DenselyDeformPath(const SkPath &path, const SkRect &effect_box,
                         const geometry::NoiseDeformParameters &parameters) {
  SkPathBuilder builder;
  builder.setFillType(path.getFillType());
  SkPathMeasure measure(path, false, 1.0F);
  do {
    const float length = measure.getLength();
    if (length <= 0.0F) {
      continue;
    }
    const int sample_count = std::max(
        4, static_cast<int>(std::ceil(length / kContourSampleSpacing)));
    bool started = false;
    for (int sample = 0; sample < sample_count; ++sample) {
      SkPoint point;
      const float distance = length * static_cast<float>(sample) / sample_count;
      if (!measure.getPosTan(distance, &point, nullptr)) {
        continue;
      }
      point = geometry::DeformPointWithNoise(point, effect_box, parameters);
      if (!started) {
        builder.moveTo(point);
        started = true;
      } else {
        builder.lineTo(point);
      }
    }
    if (started && measure.isClosed()) {
      builder.close();
    }
  } while (measure.nextContour());
  return builder.detach();
}

void DrawWaterField(SkCanvas *canvas, const SkRect &box, float time,
                    float device_scale) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kWaterColor);
  canvas->drawRect(box, paint);

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(std::max(device_scale, 1.0F));
  paint.setColor(kWaterLineColor);
  constexpr int kWaveCount = 6;
  constexpr int kSamples = 64;
  for (int wave = 0; wave < kWaveCount; ++wave) {
    SkPathBuilder builder;
    for (int sample = 0; sample <= kSamples; ++sample) {
      const float u = static_cast<float>(sample) / kSamples;
      const float base_y = box.top() + box.height() *
                                           static_cast<float>(wave + 1) /
                                           static_cast<float>(kWaveCount + 1);
      const float displacement = noise::Perlin3D(
          u * 3.2F, static_cast<float>(wave) * 1.7F, time * 0.22F, 91U);
      const SkPoint point = {std::lerp(box.left(), box.right(), u),
                             base_y + displacement * box.height() * 0.07F};
      if (sample == 0) {
        builder.moveTo(point);
      } else {
        builder.lineTo(point);
      }
    }
    canvas->drawPath(builder.detach(), paint);
  }
  paint.setColor(0x902076a2);
  paint.setStrokeWidth(std::max(1.25F * device_scale, 1.0F));
  canvas->drawRect(box, paint);
}

} // namespace

NoiseDeformFiddle::NoiseDeformFiddle() = default;
NoiseDeformFiddle::~NoiseDeformFiddle() = default;

std::vector<FiddleWidget> NoiseDeformFiddle::Widgets() const {
  return {{"intensity",
           "Distortion intensity",
           "range",
           std::to_string(intensity_),
           {},
           0.0,
           1.0,
           0.1},
          {"rotation",
           "Rotation (degrees)",
           "range",
           std::to_string(rotation_degrees_),
           {},
           0.0,
           90.0,
           1.0}};
}

bool NoiseDeformFiddle::SetInput(const std::string &name,
                                 const std::string &value) {
  char *end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0')
    return false;
  if (name == "intensity") {
    intensity_ = std::clamp(parsed, 0.0F, 1.0F);
    return true;
  }
  if (name == "rotation") {
    rotation_degrees_ = std::clamp(parsed, 0.0F, 90.0F);
    return true;
  }
  return false;
}

bool NoiseDeformFiddle::EnsureResources() {
  if (webgl_ != nullptr && typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;
  const sk_sp<SkFontMgr> manager = SkiaFontManager::Instance().FontManager();
  if (manager == nullptr || manager->countFamilies() == 0) {
    return false;
  }
  SkString family;
  manager->getFamilyName(0, &family);
  typeface_ = manager->matchFamilyStyle(family.c_str(), SkFontStyle::Bold());
  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (typeface_ == nullptr || !webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

bool NoiseDeformFiddle::Rebuild(int width, int height,
                                int configuration_index) {
  const NoiseConfiguration &configuration =
      kConfigurations[static_cast<std::size_t>(configuration_index)];
  constexpr float kSourceFontSize = 180.0F;
  SkFont font(typeface_, kSourceFontSize);
  font.setEdging(SkFont::Edging::kAntiAlias);
  font.setEmbolden(true);

  std::vector<SkPath> raw_paths;
  raw_paths.reserve(configuration.word.size());
  float cursor = 0.0F;
  for (char character : configuration.word) {
    const SkGlyphID glyph =
        font.unicharToGlyph(static_cast<unsigned char>(character));
    const std::optional<SkPath> glyph_path = font.getPath(glyph);
    SkPath positioned;
    if (glyph_path.has_value()) {
      glyph_path->transform(SkMatrix::Translate(cursor, 0.0F), &positioned);
    }
    raw_paths.push_back(positioned);
    cursor += font.measureText(&character, 1U, SkTextEncoding::kUTF8);
  }

  const SkRect raw_bounds = JoinBounds(raw_paths);
  if (raw_bounds.isEmpty()) {
    return false;
  }
  const float aspect_ratio =
      static_cast<float>(width) / std::max(static_cast<float>(height), 1.0F);
  const int row_count =
      std::clamp(static_cast<int>(std::ceil(aspect_ratio * 1.6F)), 2, 4);
  const float row_stride = raw_bounds.height() * 1.18F;
  const float total_raw_height =
      raw_bounds.height() + row_stride * static_cast<float>(row_count - 1);
  const float scale = std::min(width * 0.90F / raw_bounds.width(),
                               height * 0.86F / total_raw_height);

  letters_.clear();
  letters_.reserve(raw_paths.size() * static_cast<std::size_t>(row_count));
  for (int row = 0; row < row_count; ++row) {
    const float row_offset =
        (static_cast<float>(row) - static_cast<float>(row_count - 1) * 0.5F) *
        row_stride * scale;
    const SkMatrix placement = SkMatrix::ScaleTranslate(
        scale, scale, width * 0.5F - raw_bounds.centerX() * scale,
        height * 0.53F + row_offset - raw_bounds.centerY() * scale);
    for (std::size_t index = 0; index < raw_paths.size(); ++index) {
      SkPath path;
      raw_paths[index].transform(placement, &path);
      const std::size_t color_index =
          static_cast<std::size_t>(row) * raw_paths.size() + index;
      letters_.push_back({
          .path = path,
          .color = configuration
                       .palette[color_index % static_cast<std::size_t>(
                                                  configuration.palette_size)],
      });
    }
  }
  cached_width_ = width;
  cached_height_ = height;
  configuration_index_ = configuration_index;
  return true;
}

void NoiseDeformFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }
  time_seconds_ = time_seconds;
  const int width = PixelWidth();
  const int height = PixelHeight();
  const int configuration_index =
      static_cast<int>(time_seconds / kConfigurationSeconds) %
      static_cast<int>(kConfigurations.size());
  if ((width != cached_width_ || height != cached_height_ ||
       configuration_index != configuration_index_) &&
      !Rebuild(width, height, configuration_index)) {
    return;
  }
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);
  if (!webgl_->FlushAndPresent().success) {
    std::cerr << "[cc-engine/stderr] Noise Deform could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

void NoiseDeformFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  canvas->clear(kCanvasColor);
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const float logical_scale =
      std::clamp(std::sqrt((width / std::max(device_scale, 0.001F)) / 925.0F),
                 0.82F, 1.35F);
  const float ui_scale = device_scale * logical_scale;
  const float time = static_cast<float>(time_seconds_);
  const SkRect effect_box = geometry::AnimatedNoiseEffectBox(
      static_cast<float>(width), static_cast<float>(height), time);
  DrawWaterField(canvas, effect_box, time, device_scale);

  geometry::NoiseDeformParameters parameters;
  parameters.time = time;
  parameters.horizontal_amplitude = width * 0.156F * intensity_;
  parameters.vertical_amplitude = 0.0F;
  parameters.direction_radians =
      rotation_degrees_ * std::numbers::pi_v<float> / 180.0F;
  parameters.seed = 0x4e4f4953U;

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  const SkPath box_path = SkPathBuilder().addRect(effect_box).detach();
  for (const NoiseDeformLetter &letter : letters_) {
    const std::optional<SkPath> outside =
        Op(letter.path, box_path, kDifference_SkPathOp);
    const std::optional<SkPath> inside =
        Op(letter.path, box_path, kIntersect_SkPathOp);
    paint.setColor(letter.color);
    if (!outside.has_value() || !inside.has_value()) {
      canvas->drawPath(letter.path, paint);
      continue;
    }
    canvas->drawPath(*outside, paint);
    if (!inside->isEmpty()) {
      canvas->drawPath(DenselyDeformPath(*inside, effect_box, parameters),
                       paint);
    }
  }

  const float elapsed = std::fmod(time, kConfigurationSeconds);
  const float remaining_fraction = 1.0F - elapsed / kConfigurationSeconds;
  const int remaining_seconds =
      std::max(1, static_cast<int>(std::ceil(kConfigurationSeconds - elapsed)));
  const std::string remaining_label =
      std::to_string(remaining_seconds) + "s left";
  const float slider_width = std::min(width * 0.22F, 170.0F * ui_scale);
  const float slider_height = 28.0F * ui_scale;
  const SkRect slider =
      SkRect::MakeXYWH(width - slider_width - 14.0F * ui_scale,
                       13.0F * ui_scale, slider_width, slider_height);
  SkFont slider_font(typeface_, 10.0F * ui_scale);
  graphics::canvas_widgets::OneSidedSliderStyle slider_style;
  slider_style.background_color = 0xe6111827;
  slider_style.track_color = 0xff27364a;
  slider_style.fill_color = 0xff20a6dd;
  slider_style.corner_radius = 4.0F * ui_scale;
  slider_style.outer_padding = 5.0F * ui_scale;
  graphics::canvas_widgets::DrawOneSidedSlider(
      canvas, slider, remaining_fraction, remaining_label, slider_font,
      slider_style);
}
