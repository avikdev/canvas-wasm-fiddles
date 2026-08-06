#include "fiddles/contour_lines_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <random>

#include "geometry/contour_regions.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "utils/color_utils.h"
#include "utils/perlin_noise.h"

namespace {

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
constexpr float kContourStrokeWidth = 1.0F;
constexpr SkColor kCanvasColor = 0xfff7f4ee;

template <std::size_t LevelCount>
constexpr std::array<float, LevelCount> MakeContourLevels() {
  std::array<float, LevelCount> levels;
  for (std::size_t index = 0; index < levels.size(); ++index) {
    levels[index] =
        static_cast<float>(index + 1U) / static_cast<float>(levels.size() + 1U);
  }
  return levels;
}

geometry::ScalarGrid BuildScalarGrid(const SkRect &bounds, float device_scale,
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
  const float noise_scale_multiplier = std::clamp(
      kNoiseReferenceShortEdge / std::max(1.0F, logical_canvas_short_edge),
      1.0F, kMaximumNoiseScaleMultiplier);
  const float noise_spatial_scale = kNoiseSpatialScale * noise_scale_multiplier;
  for (int row = 0; row < grid.row_count; ++row) {
    for (int column = 0; column < grid.column_count; ++column) {
      const SkPoint point = grid.PointAt(column, row);
      const float logical_x = point.fX / device_scale;
      const float logical_y = point.fY / device_scale;
      const float value = noise::FractalPerlin3D(
          logical_x * noise_spatial_scale, logical_y * noise_spatial_scale,
          time_coordinate, kNoiseOctaveCount, kNoiseLacunarity,
          kNoisePersistence, seed);
      grid.values[static_cast<std::size_t>(row) *
                      static_cast<std::size_t>(grid.column_count) +
                  static_cast<std::size_t>(column)] = value;
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
    }
  }

  // Normalizing the current coherent slice makes every configured level useful
  // even though a finite Perlin sample rarely reaches the theoretical extrema.
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

} // namespace

ContourLinesFiddle::ContourLinesFiddle() : field_seed_(std::random_device{}()) {
  for (std::size_t index = 0; index < band_colors_.size(); ++index) {
    const float hue = 18.0F + 360.0F * static_cast<float>(index) /
                                  static_cast<float>(band_colors_.size());
    band_colors_[index] = color_utils::FromHsl(hue, 0.82F, 0.62F);
  }
}

ContourLinesFiddle::~ContourLinesFiddle() = default;

bool ContourLinesFiddle::EnsureResources() {
  if (webgl_ != nullptr) {
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
  webgl_ = std::move(webgl);
  return true;
}

void ContourLinesFiddle::Render(double time_seconds) {
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
    std::cerr << "[cc-engine/stderr] Contour Lines could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

bool ContourLinesFiddle::UpdateState(double time_seconds, int, int) {
  time_seconds_ = time_seconds;
  return true;
}

void ContourLinesFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  const float device_scale = static_cast<float>(width / std::max(1.0, Width()));
  const float padding =
      std::min(kCanvasPadding * device_scale,
               std::max(2.0F, std::min(width, height) * 0.12F));
  const SkRect field_bounds =
      SkRect::MakeLTRB(padding, padding, static_cast<float>(width) - padding,
                       static_cast<float>(height) - padding);
  if (field_bounds.isEmpty()) {
    return;
  }

  constexpr auto kLevels = MakeContourLevels<kContourLevelCount>();
  const float logical_canvas_short_edge =
      static_cast<float>(std::min(Width(), Height()));
  const geometry::ScalarGrid grid =
      BuildScalarGrid(field_bounds, device_scale, logical_canvas_short_edge,
                      time_seconds_, field_seed_);
  const std::optional<geometry::ContourRegionSet> regions =
      geometry::BuildInclusiveContourRegions(grid, kLevels,
                                             kContourSplineTension);
  if (!regions.has_value()) {
    canvas->clear(kCanvasColor);
    return;
  }

  canvas->clear(kCanvasColor);
  canvas->save();
  canvas->clipRect(field_bounds, SkClipOp::kIntersect, true);

  SkPaint paint;
  paint.setAntiAlias(true);
  for (std::size_t region_index = regions->size(); region_index-- > 0U;) {
    const SkPath *region = regions->InclusiveRegion(region_index);
    if (region == nullptr || region->isEmpty()) {
      continue;
    }
    const std::size_t color_index = regions->size() - region_index - 1U;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor4f(band_colors_[color_index]);
    canvas->drawPath(*region, paint);
  }

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kContourStrokeWidth * device_scale);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  paint.setColor(SK_ColorBLACK);
  for (std::size_t region_index = 0; region_index < regions->size();
       ++region_index) {
    const SkPath *region = regions->InclusiveRegion(region_index);
    if (region != nullptr && !region->isEmpty()) {
      canvas->drawPath(*region, paint);
    }
  }

  // TODO: When the UI requests exactly one elevation band, draw
  // regions->ExclusiveRegion(index) so that isolated compound region retains
  // its holes without requiring the other inclusive layers.
  canvas->restore();
}
