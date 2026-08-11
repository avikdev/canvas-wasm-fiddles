#include "fiddles/contour_composite_fiddle.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>

#include "geometry/contour_regions.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPicture.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkTypeface.h"
#include "text/skia_font_manager.h"
#include "utils/perlin_noise.h"

namespace {

constexpr std::string_view kThresholdText =
    "0.70, 0.65, 0.35, 0.30, 0.20,";
constexpr float kCanvasPadding = 24.0F;
constexpr float kNoiseSpatialScale = 0.003F;
constexpr float kNoiseTimeScale = 0.03F;
constexpr float kNoiseReferenceShortEdge = 600.0F;
constexpr float kMaximumNoiseScaleMultiplier = 2.25F;
constexpr float kTargetGridCellsAcrossShortEdge = 44.0F;
constexpr float kMinimumGridCellSize = 8.0F;
constexpr float kMaximumGridCellSize = 12.0F;
constexpr int kNoiseOctaveCount = 4;
constexpr float kNoiseLacunarity = 2.0F;
constexpr float kNoisePersistence = 0.54F;
constexpr float kContourSplineTension = 0.1F;
constexpr float kCoreRadiusFraction = 0.1F;
constexpr float kFalloffCoefficient = 0.995F;
constexpr float kFalloffRateScale = 0.5F;
constexpr float kHatchSpacingScale = 4.0F;
constexpr float kHatchPhase = 2.5F;
constexpr SkColor kBlue = 0xff30afff;
constexpr SkColor kTextBlue = 0xff0d47a1;

std::vector<float> ParseThresholds(std::string_view text) {
  const std::string storage(text);
  const char *cursor = storage.c_str();
  std::vector<float> result;
  while (*cursor != '\0') {
    char *end = nullptr;
    const float value = std::strtof(cursor, &end);
    if (end == cursor) {
      ++cursor;
      continue;
    }
    if (std::isfinite(value)) {
      result.push_back(value);
    }
    cursor = end;
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

geometry::ScalarGrid BuildNoiseGrid(const SkRect &bounds, float device_scale,
                                    float logical_canvas_short_edge,
                                    double time_seconds, std::uint32_t seed) {
  geometry::ScalarGrid grid;
  grid.bounds = bounds;
  const float logical_cell_size =
      std::clamp(logical_canvas_short_edge / kTargetGridCellsAcrossShortEdge,
                 kMinimumGridCellSize, kMaximumGridCellSize);
  const float target_cell_size = logical_cell_size * device_scale;
  grid.column_count = std::max(
      2, static_cast<int>(std::ceil(bounds.width() / target_cell_size)) + 1);
  grid.row_count = std::max(
      2, static_cast<int>(std::ceil(bounds.height() / target_cell_size)) + 1);
  grid.values.resize(static_cast<std::size_t>(grid.column_count) *
                     static_cast<std::size_t>(grid.row_count));

  float minimum = std::numeric_limits<float>::infinity();
  float maximum = -std::numeric_limits<float>::infinity();
  const float time_coordinate =
      static_cast<float>(time_seconds) * kNoiseTimeScale;
  const float scale_multiplier = std::clamp(
      kNoiseReferenceShortEdge / std::max(1.0F, logical_canvas_short_edge),
      1.0F, kMaximumNoiseScaleMultiplier);
  const float spatial_scale = kNoiseSpatialScale * scale_multiplier;
  for (int row = 0; row < grid.row_count; ++row) {
    for (int column = 0; column < grid.column_count; ++column) {
      const SkPoint point = grid.PointAt(column, row);
      const float value = noise::FractalPerlin3D(
          point.fX / device_scale * spatial_scale,
          point.fY / device_scale * spatial_scale, time_coordinate,
          kNoiseOctaveCount, kNoiseLacunarity, kNoisePersistence, seed);
      const std::size_t index =
          static_cast<std::size_t>(row) *
              static_cast<std::size_t>(grid.column_count) +
          static_cast<std::size_t>(column);
      grid.values[index] = value;
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
    }
  }

  const float range = maximum - minimum;
  if (range <= 0.000001F || !std::isfinite(range)) {
    std::fill(grid.values.begin(), grid.values.end(), 0.5F);
  } else {
    for (float &value : grid.values) {
      value = std::clamp((value - minimum) / range, 0.0F, 1.0F);
    }
  }
  return grid;
}

void AddRadialCore(geometry::ScalarGrid *grid, SkPoint center, float radius) {
  for (int row = 0; row < grid->row_count; ++row) {
    for (int column = 0; column < grid->column_count; ++column) {
      const SkPoint point = grid->PointAt(column, row);
      const float distance =
          std::hypot(point.fX - center.fX, point.fY - center.fY);
      float radial_value = 1.0F;
      if (distance > radius) {
        const float normalized_distance = (distance - radius) / radius;
        radial_value =
            std::exp(-kFalloffRateScale *
                     (kFalloffCoefficient / (1.0F - kFalloffCoefficient)) *
                     normalized_distance * normalized_distance);
      }
      const std::size_t index =
          static_cast<std::size_t>(row) *
              static_cast<std::size_t>(grid->column_count) +
          static_cast<std::size_t>(column);
      grid->values[index] =
          std::clamp(grid->values[index] + radial_value, 0.0F, 1.0F);
    }
  }
}

sk_sp<SkShader> MakeHatchShader(float density) {
  const float tile_size = kHatchSpacingScale / density;
  SkPictureRecorder recorder;
  SkCanvas *tile_canvas = recorder.beginRecording(tile_size, tile_size);
  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setColor(kBlue);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(3.0F);
  stroke.setStrokeCap(SkPaint::kSquare_Cap);

  // Keep one global phase for every density so halving the tile size retains
  // all existing lines and inserts a line halfway between each pair. Record
  // both periodic segments well beyond the tile bounds so their clipped ends
  // join cleanly when the picture shader repeats.
  tile_canvas->drawLine(-tile_size, kHatchPhase + tile_size,
                        kHatchPhase + tile_size, -tile_size, stroke);
  tile_canvas->drawLine(-tile_size, kHatchPhase + 2.0F * tile_size,
                        kHatchPhase + 2.0F * tile_size, -tile_size, stroke);
  const sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  return picture->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                             SkFilterMode::kLinear);
}

void ConfigureBandPaint(std::size_t color_index,
                        const std::array<sk_sp<SkShader>, 4> &patterns,
                        SkPaint *paint) {
  paint->setShader(nullptr);
  paint->setStyle(SkPaint::kFill_Style);
  if (color_index == 5U) {
    paint->setColor(kBlue);
  } else if (color_index >= 1U && color_index <= 4U) {
    paint->setShader(patterns[color_index - 1U]);
  }
}

void DrawCenterLabel(SkCanvas *canvas, SkPoint center, float radius,
                     const sk_sp<SkTypeface> &typeface) {
  constexpr std::string_view kLabel = "Clear Zone";
  SkFont font(typeface, radius * 0.42F);
  font.setEdging(SkFont::Edging::kAntiAlias);
  SkRect bounds;
  float width = font.measureText(kLabel.data(), kLabel.size(),
                                 SkTextEncoding::kUTF8, &bounds);
  const float maximum_width = radius * 1.55F;
  if (width > maximum_width) {
    font.setSize(font.getSize() * maximum_width / width);
    width = font.measureText(kLabel.data(), kLabel.size(),
                             SkTextEncoding::kUTF8, &bounds);
  }
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(kTextBlue);
  canvas->drawSimpleText(kLabel.data(), kLabel.size(), SkTextEncoding::kUTF8,
                         center.fX - width * 0.5F,
                         center.fY - (bounds.top() + bounds.bottom()) * 0.5F,
                         font, paint);
}

} // namespace

ContourCompositeFiddle::ContourCompositeFiddle()
    : thresholds_(ParseThresholds(kThresholdText)),
      field_seed_(std::random_device{}()) {}

ContourCompositeFiddle::~ContourCompositeFiddle() = default;

bool ContourCompositeFiddle::EnsureResources() {
  if (webgl_ != nullptr && typeface_ != nullptr &&
      std::all_of(pattern_shaders_.begin(), pattern_shaders_.end(),
                  [](const sk_sp<SkShader> &shader) {
                    return shader != nullptr;
                  })) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  const sk_sp<SkFontMgr> font_manager =
      SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    return false;
  }
  SkString family;
  font_manager->getFamilyName(0, &family);
  typeface_ =
      font_manager->matchFamilyStyle(family.c_str(), SkFontStyle::Normal());
  if (typeface_ == nullptr || !BuildPatternShaders()) {
    return false;
  }

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

bool ContourCompositeFiddle::BuildPatternShaders() {
  constexpr std::array<float, 4> kDensities = {0.1F, 0.2F, 0.4F, 0.8F};
  for (std::size_t index = 0; index < kDensities.size(); ++index) {
    pattern_shaders_[index] = MakeHatchShader(kDensities[index]);
  }
  return std::all_of(pattern_shaders_.begin(), pattern_shaders_.end(),
                     [](const sk_sp<SkShader> &shader) {
                       return shader != nullptr;
                     });
}

void ContourCompositeFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }
  time_seconds_ = time_seconds;
  const int width = PixelWidth();
  const int height = PixelHeight();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);
  if (!webgl_->FlushAndPresent().success) {
    std::cerr << "[cc-engine/stderr] Contour 2 could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

void ContourCompositeFiddle::DrawFrame(SkCanvas *canvas, int width,
                                       int height) {
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const float padding =
      std::min(kCanvasPadding * device_scale,
               std::max(2.0F, std::min(width, height) * 0.12F));
  const SkRect field_bounds =
      SkRect::MakeLTRB(padding, padding, static_cast<float>(width) - padding,
                       static_cast<float>(height) - padding);
  canvas->clear(SK_ColorWHITE);
  if (field_bounds.isEmpty()) {
    return;
  }

  geometry::ScalarGrid grid =
      BuildNoiseGrid(field_bounds, device_scale,
                     static_cast<float>(std::min(Width(), Height())),
                     time_seconds_, field_seed_);
  const SkPoint center = field_bounds.center();
  const float radius =
      kCoreRadiusFraction * static_cast<float>(std::min(width, height));
  AddRadialCore(&grid, center, radius);
  const std::optional<geometry::ContourRegionSet> regions =
      geometry::BuildInclusiveContourRegions(grid, thresholds_,
                                             kContourSplineTension);
  if (!regions.has_value()) {
    return;
  }

  canvas->save();
  canvas->clipRect(field_bounds, SkClipOp::kIntersect, true);
  SkPaint paint;
  paint.setAntiAlias(true);
  for (std::size_t region_index = 0; region_index < regions->size();
       ++region_index) {
    const std::size_t color_index = region_index;
    if (color_index == 0U) {
      continue;
    }
    const std::optional<SkPath> band = regions->ExclusiveRegion(region_index);
    if (!band.has_value() || band->isEmpty()) {
      continue;
    }
    ConfigureBandPaint(color_index, pattern_shaders_, &paint);
    canvas->drawPath(*band, paint);
  }
  canvas->restore();

  DrawCenterLabel(canvas, center, radius, typeface_);
}
