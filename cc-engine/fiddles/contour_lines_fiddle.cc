#include "fiddles/contour_lines_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include "geometry/catmull_rom_spline.h"
#include "geometry/marching_squares.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
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
constexpr float kTargetGridCellsAcrossShortEdge = 28.0F;
constexpr float kMinimumGridCellSize = 10.0F;
constexpr float kMaximumGridCellSize = 18.0F;
constexpr int kNoiseOctaveCount = 4;
constexpr float kNoiseLacunarity = 2.0F;
constexpr float kNoisePersistence = 0.54F;
constexpr float kContourTension = 0.25F;
constexpr float kContourStrokeWidth = 1.0F;
constexpr SkColor kCanvasColor = 0xfff7f4ee;

struct ScalarVertex {
  SkPoint point;
  float value = 0.0F;
};

template <std::size_t LevelCount>
int BandIndex(float value, const std::array<float, LevelCount> &levels) {
  return static_cast<int>(
      std::upper_bound(levels.begin(), levels.end(), value) - levels.begin());
}

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

std::vector<ScalarVertex> ClipByValue(const std::vector<ScalarVertex> &input,
                                      float threshold, bool keep_above) {
  std::vector<ScalarVertex> output;
  if (input.empty()) {
    return output;
  }
  output.reserve(input.size() + 1U);
  const auto inside = [threshold, keep_above](float value) {
    return keep_above ? value >= threshold : value <= threshold;
  };
  for (std::size_t index = 0; index < input.size(); ++index) {
    const ScalarVertex &current = input[index];
    const ScalarVertex &previous =
        input[(index + input.size() - 1U) % input.size()];
    const bool current_inside = inside(current.value);
    const bool previous_inside = inside(previous.value);
    if (current_inside != previous_inside) {
      const float denominator = current.value - previous.value;
      const float amount =
          std::abs(denominator) <= 0.000001F
              ? 0.5F
              : std::clamp((threshold - previous.value) / denominator, 0.0F,
                           1.0F);
      output.push_back({
          {std::lerp(previous.point.fX, current.point.fX, amount),
           std::lerp(previous.point.fY, current.point.fY, amount)},
          threshold,
      });
    }
    if (current_inside) {
      output.push_back(current);
    }
  }
  return output;
}

template <std::size_t LevelCount>
void AppendTriangleBands(const std::array<ScalarVertex, 3> &triangle,
                         const std::array<float, LevelCount> &levels,
                         std::array<SkPathBuilder, LevelCount + 1U> *builders) {
  const float minimum =
      std::min({triangle[0].value, triangle[1].value, triangle[2].value});
  const float maximum =
      std::max({triangle[0].value, triangle[1].value, triangle[2].value});
  const int first_band = BandIndex(minimum, levels);
  const int last_band = BandIndex(maximum, levels);
  for (int band = first_band; band <= last_band; ++band) {
    std::vector<ScalarVertex> polygon(triangle.begin(), triangle.end());
    if (band > 0) {
      polygon = ClipByValue(polygon, levels[static_cast<std::size_t>(band - 1)],
                            true);
    }
    if (band < static_cast<int>(levels.size())) {
      polygon =
          ClipByValue(polygon, levels[static_cast<std::size_t>(band)], false);
    }
    if (polygon.size() < 3U) {
      continue;
    }
    SkPathBuilder &builder = (*builders)[static_cast<std::size_t>(band)];
    builder.moveTo(polygon.front().point);
    for (std::size_t index = 1; index < polygon.size(); ++index) {
      builder.lineTo(polygon[index].point);
    }
    builder.close();
  }
}

template <std::size_t LevelCount>
std::array<SkPath, LevelCount + 1U>
BuildBandPaths(const geometry::ScalarGrid &grid,
               const std::array<float, LevelCount> &levels) {
  std::array<SkPathBuilder, LevelCount + 1U> builders;
  for (int row = 0; row < grid.row_count - 1; ++row) {
    for (int column = 0; column < grid.column_count - 1; ++column) {
      const ScalarVertex top_left = {grid.PointAt(column, row),
                                     grid.ValueAt(column, row)};
      const ScalarVertex top_right = {grid.PointAt(column + 1, row),
                                      grid.ValueAt(column + 1, row)};
      const ScalarVertex bottom_right = {grid.PointAt(column + 1, row + 1),
                                         grid.ValueAt(column + 1, row + 1)};
      const ScalarVertex bottom_left = {grid.PointAt(column, row + 1),
                                        grid.ValueAt(column, row + 1)};
      AppendTriangleBands(std::array{top_left, top_right, bottom_right}, levels,
                          &builders);
      AppendTriangleBands(std::array{top_left, bottom_right, bottom_left},
                          levels, &builders);
    }
  }

  std::array<SkPath, LevelCount + 1U> paths;
  for (std::size_t band = 0; band < paths.size(); ++band) {
    paths[band] = builders[band].detach();
  }
  return paths;
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
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }

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
                      time_seconds, field_seed_);
  const auto band_paths = BuildBandPaths(grid, kLevels);

  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kCanvasColor);
  canvas->save();
  canvas->clipRect(field_bounds, SkClipOp::kIntersect, true);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  for (std::size_t band = 0; band < band_paths.size(); ++band) {
    paint.setColor4f(band_colors_[band]);
    canvas->drawPath(band_paths[band], paint);
  }

  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kContourStrokeWidth * device_scale);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeJoin(SkPaint::kRound_Join);
  paint.setColor(SK_ColorBLACK);
  geometry::CatmullRomOptions spline_options;
  spline_options.tension = kContourTension;
  for (float level : kLevels) {
    const std::vector<geometry::ContourPolyline> contours =
        geometry::ExtractMarchingSquaresContours(grid, level);
    for (const geometry::ContourPolyline &contour : contours) {
      spline_options.closed = contour.closed;
      const SkPath path =
          geometry::CatmullRomToCubicPath(contour.points, spline_options);
      if (!path.isEmpty()) {
        canvas->drawPath(path, paint);
      }
    }
  }

  canvas->restore();
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kContourStrokeWidth * device_scale);
  paint.setColor(SK_ColorBLACK);
  canvas->drawRect(field_bounds, paint);

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Contour Lines could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}
