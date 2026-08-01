#include "fiddles/shape_intersection_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "graphics/webgl_canvas_context.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "include/pathops/SkPathOps.h"
#include "text/skia_font_manager.h"

namespace {

constexpr int kOctopusCount = 5;
constexpr int kHoledPolygonCount = 4;
constexpr int kPlusCount = 1;
constexpr int kCircleCount = 3;
constexpr int kBlobCount =
    kOctopusCount + kHoledPolygonCount + kPlusCount + kCircleCount;
constexpr int kPieceColorCount = 17;
constexpr SkColor kCanvasColor = SkColorSetRGB(244, 232, 210);
constexpr std::array<SkColor, kPieceColorCount> kPieceColors = {
    0xff4361ee, 0xff3a86ff, 0xff00b4d8, 0xff06d6a0, 0xff70e000, 0xffffbe0b,
    0xffff9f1c, 0xffff5d8f, 0xfff72585, 0xffb5179e, 0xff8338ec, 0xffef476f,
    0xffff1744, 0xff651fff, 0xff00e5ff, 0xffaeea00, 0xffff3d00};

struct BlobShape {
  SkPath path;
  std::uint64_t id;
};

struct Region {
  SkPath path;
  std::uint64_t id;
};

std::uint32_t Hash(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

std::uint64_t HashPieceId(std::uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

std::uint64_t CombinePieceIds(std::uint64_t first, std::uint64_t second) {
  if (first > second) {
    std::swap(first, second);
  }
  return HashPieceId(first ^ (HashPieceId(second) + 0x517cc1b727220a95ULL));
}

float HashUnit(int x, int y, std::uint32_t seed) {
  const std::uint32_t mixed =
      Hash(static_cast<std::uint32_t>(x) * 0x9e3779b9U ^
           static_cast<std::uint32_t>(y) * 0x85ebca6bU ^ seed);
  return static_cast<float>(mixed & 0x00ffffffU) /
         static_cast<float>(0x01000000U);
}

float SmoothStep(float value) { return value * value * (3.0F - 2.0F * value); }

float Noise2D(float x, float y, std::uint32_t seed) {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const float tx = SmoothStep(x - static_cast<float>(x0));
  const float ty = SmoothStep(y - static_cast<float>(y0));
  const float top =
      std::lerp(HashUnit(x0, y0, seed), HashUnit(x0 + 1, y0, seed), tx);
  const float bottom =
      std::lerp(HashUnit(x0, y0 + 1, seed), HashUnit(x0 + 1, y0 + 1, seed), tx);
  return std::lerp(top, bottom, ty) * 2.0F - 1.0F;
}

SkColor4f ToColor4f(SkColor color) {
  constexpr float kScale = 1.0F / 255.0F;
  return {SkColorGetR(color) * kScale, SkColorGetG(color) * kScale,
          SkColorGetB(color) * kScale, 1.0F};
}

SkColor4f ColorForPieceId(std::uint64_t id) {
  const std::size_t color_index =
      static_cast<std::size_t>(HashPieceId(id)) % kPieceColors.size();
  return ToColor4f(kPieceColors[color_index]);
}

SkPoint EdgePoint(int side, float along, float outside, float width,
                  float height) {
  switch (side) {
  case 0:
    return {-outside, height * along};
  case 1:
    return {width * along, -outside};
  case 2:
    return {width + outside, height * along};
  default:
    return {width * along, height + outside};
  }
}

SkPoint TransformBlobPoint(float local_x, float local_y, float center_x,
                           float center_y, float rotation, float stretch) {
  const float scaled_x = local_x * stretch;
  const float scaled_y = local_y / stretch;
  const float cosine = std::cos(rotation);
  const float sine = std::sin(rotation);
  return {center_x + scaled_x * cosine - scaled_y * sine,
          center_y + scaled_x * sine + scaled_y * cosine};
}

SkPoint TentaclePoint(float radial, float lateral, float angle, float center_x,
                      float center_y, float rotation, float stretch) {
  const float local_x = std::cos(angle) * radial - std::sin(angle) * lateral;
  const float local_y = std::sin(angle) * radial + std::cos(angle) * lateral;
  return TransformBlobPoint(local_x, local_y, center_x, center_y, rotation,
                            stretch);
}

struct Tentacle {
  SkPoint root_left;
  SkPoint control1_left;
  SkPoint control2_left;
  SkPoint tip_left;
  SkPoint cap;
  SkPoint tip_right;
  SkPoint control2_right;
  SkPoint control1_right;
  SkPoint root_right;
  SkPoint connector_before;
  SkPoint connector_after;
};

SkPath GenerateTentacledBlobPath(float center_x, float center_y,
                                 float base_radius, float rotation,
                                 float stretch, float shape_time,
                                 std::uint32_t seed) {
  constexpr int kMaxTentacles = 5;
  const int tentacle_count = 3 + static_cast<int>(Hash(seed + 91U) % 3U);
  const float sector =
      std::numbers::pi_v<float> * 2.0F / static_cast<float>(tentacle_count);
  const float phase = HashUnit(73, 29, seed) * sector;
  std::array<Tentacle, kMaxTentacles> tentacles;

  for (int index = 0; index < tentacle_count; ++index) {
    const float angle = phase + static_cast<float>(index) * sector;
    const float body_radius =
        base_radius * (0.62F + HashUnit(index, 13, seed) * 0.24F);
    const float length =
        base_radius * (1.70F + HashUnit(index, 31, seed) * 1.65F);
    const float root_half_width =
        base_radius * (0.24F + HashUnit(index, 47, seed) * 0.10F);
    const float tip_half_width =
        base_radius * (0.075F + HashUnit(index, 59, seed) * 0.045F);
    const float bend_direction =
        ((static_cast<unsigned int>(index) + (seed & 1U)) % 2U == 0U) ? 1.0F
                                                                      : -1.0F;
    const float bend_noise =
        1.0F + Noise2D(index * 0.83F, shape_time, seed + 0x85ebca6bU) * 0.16F;
    const float bend = bend_direction * base_radius *
                       (0.28F + HashUnit(index, 71, seed) * 0.22F) *
                       (0.82F + 0.18F * std::sin(shape_time + index * 1.37F)) *
                       bend_noise;
    const float control1_radius = std::lerp(body_radius, length, 0.30F);
    const float control2_radius = std::lerp(body_radius, length, 0.72F);
    const float tip_lateral = bend * 0.68F;

    Tentacle &tentacle = tentacles[index];
    tentacle.root_left = TentaclePoint(body_radius, -root_half_width, angle,
                                       center_x, center_y, rotation, stretch);
    tentacle.control1_left =
        TentaclePoint(control1_radius, bend - root_half_width * 0.72F, angle,
                      center_x, center_y, rotation, stretch);
    tentacle.control2_left =
        TentaclePoint(control2_radius, -bend * 0.58F - tip_half_width * 1.7F,
                      angle, center_x, center_y, rotation, stretch);
    tentacle.tip_left =
        TentaclePoint(length, tip_lateral - tip_half_width, angle, center_x,
                      center_y, rotation, stretch);
    tentacle.cap = TentaclePoint(length + tip_half_width * 1.65F, tip_lateral,
                                 angle, center_x, center_y, rotation, stretch);
    tentacle.tip_right =
        TentaclePoint(length, tip_lateral + tip_half_width, angle, center_x,
                      center_y, rotation, stretch);
    tentacle.control2_right =
        TentaclePoint(control2_radius, -bend * 0.58F + tip_half_width * 1.7F,
                      angle, center_x, center_y, rotation, stretch);
    tentacle.control1_right =
        TentaclePoint(control1_radius, bend + root_half_width * 0.72F, angle,
                      center_x, center_y, rotation, stretch);
    tentacle.root_right = TentaclePoint(body_radius, root_half_width, angle,
                                        center_x, center_y, rotation, stretch);
    tentacle.connector_before =
        TentaclePoint(body_radius, 0.0F, angle - sector * 0.31F, center_x,
                      center_y, rotation, stretch);
    tentacle.connector_after =
        TentaclePoint(body_radius, 0.0F, angle + sector * 0.31F, center_x,
                      center_y, rotation, stretch);
  }

  SkPathBuilder builder;
  builder.moveTo(tentacles[0].root_left);
  for (int index = 0; index < tentacle_count; ++index) {
    const Tentacle &tentacle = tentacles[index];
    if (index > 0) {
      const Tentacle &previous = tentacles[index - 1];
      builder.cubicTo(previous.connector_after, tentacle.connector_before,
                      tentacle.root_left);
    }
    builder.cubicTo(tentacle.control1_left, tentacle.control2_left,
                    tentacle.tip_left);
    builder.quadTo(tentacle.cap, tentacle.tip_right);
    builder.cubicTo(tentacle.control2_right, tentacle.control1_right,
                    tentacle.root_right);
  }
  builder.cubicTo(tentacles[tentacle_count - 1].connector_after,
                  tentacles[0].connector_before, tentacles[0].root_left);
  builder.close();
  return builder.detach();
}

void AppendRegularPolygonContour(SkPathBuilder *builder, int side_count,
                                 float radius, float phase, float center_x,
                                 float center_y, float rotation,
                                 float stretch) {
  for (int side = 0; side < side_count; ++side) {
    const float angle = phase + static_cast<float>(side) *
                                    std::numbers::pi_v<float> * 2.0F /
                                    static_cast<float>(side_count);
    const SkPoint point =
        TransformBlobPoint(std::cos(angle) * radius, std::sin(angle) * radius,
                           center_x, center_y, rotation, stretch);
    if (side == 0) {
      builder->moveTo(point);
    } else {
      builder->lineTo(point);
    }
  }
  builder->close();
}

SkPath GenerateHoledPolygonPath(int side_count, float center_x, float center_y,
                                float radius, float rotation, float stretch) {
  SkPathBuilder builder;
  builder.setFillType(SkPathFillType::kEvenOdd);
  const float phase = side_count == 4 ? std::numbers::pi_v<float> * 0.25F
                                      : -std::numbers::pi_v<float> * 0.5F;
  AppendRegularPolygonContour(&builder, side_count, radius, phase, center_x,
                              center_y, rotation, stretch);
  AppendRegularPolygonContour(&builder, side_count, radius * 0.43F, phase,
                              center_x, center_y, rotation, stretch);
  return builder.detach();
}

SkPath GeneratePlusPath(float center_x, float center_y, float radius,
                        float rotation, float stretch) {
  const float half_length = radius * 1.75F;
  const float half_width = radius * 0.38F;
  constexpr int kPointCount = 12;
  const std::array<SkPoint, kPointCount> local_points = {{
      {-half_width, -half_length},
      {half_width, -half_length},
      {half_width, -half_width},
      {half_length, -half_width},
      {half_length, half_width},
      {half_width, half_width},
      {half_width, half_length},
      {-half_width, half_length},
      {-half_width, half_width},
      {-half_length, half_width},
      {-half_length, -half_width},
      {-half_width, -half_width},
  }};

  SkPathBuilder builder;
  for (int index = 0; index < kPointCount; ++index) {
    const SkPoint point =
        TransformBlobPoint(local_points[index].fX, local_points[index].fY,
                           center_x, center_y, rotation, stretch);
    if (index == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

SkPath GenerateCirclePath(float center_x, float center_y, float radius) {
  SkPathBuilder builder;
  builder.addCircle(center_x, center_y, radius);
  return builder.detach();
}

BlobShape BuildBlob(int blob_index, double time_seconds, float width,
                    float height) {
  const float duration = 30.0F + static_cast<float>(blob_index % 4) * 3.5F;
  const float journey =
      static_cast<float>(time_seconds) / duration + blob_index * 0.113F;
  const int cycle = static_cast<int>(std::floor(journey));
  const float progress = journey - static_cast<float>(cycle);
  const std::uint32_t seed =
      Hash(static_cast<std::uint32_t>(blob_index + 1) * 0x45d9f3bU ^
           static_cast<std::uint32_t>(cycle + 17) * 0x27d4eb2dU);
  const float shortest = std::min(width, height);
  const float base_radius =
      shortest * (0.10F + HashUnit(blob_index, cycle, seed) * 0.06F);
  const float extent = base_radius * 3.35F;
  const int entry_side = static_cast<int>(Hash(seed) % 4U);
  const int exit_side =
      (entry_side + 1 + static_cast<int>(Hash(seed + 0x6d2b79f5U) % 3U)) % 4;
  const float entry_along =
      0.14F + HashUnit(blob_index + 11, cycle, seed) * 0.72F;
  const float exit_along =
      0.14F + HashUnit(blob_index + 29, cycle, seed) * 0.72F;
  const SkPoint entry =
      EdgePoint(entry_side, entry_along, extent, width, height);
  const SkPoint exit = EdgePoint(exit_side, exit_along, extent, width, height);
  const bool approaching_center = progress <= 0.5F;
  const float segment_progress =
      approaching_center ? progress * 2.0F : (progress - 0.5F) * 2.0F;
  const float segment_eased = SmoothStep(segment_progress);
  const SkPoint canvas_center = {width * 0.5F, height * 0.5F};
  const SkPoint segment_start = approaching_center ? entry : canvas_center;
  const SkPoint segment_end = approaching_center ? canvas_center : exit;
  const float direction_x = segment_end.fX - segment_start.fX;
  const float direction_y = segment_end.fY - segment_start.fY;
  const float direction_length =
      std::max(1.0F, std::hypot(direction_x, direction_y));
  const float curve_direction =
      ((blob_index + cycle + (approaching_center ? 0 : 1)) % 2 == 0) ? 1.0F
                                                                     : -1.0F;
  const float curve_amount =
      shortest * (0.05F + HashUnit(blob_index + 37, cycle, seed) * 0.08F) *
      std::sin(segment_progress * std::numbers::pi_v<float>) * curve_direction;
  const float center_x =
      std::lerp(segment_start.fX, segment_end.fX, segment_eased) -
      direction_y / direction_length * curve_amount;
  const float center_y =
      std::lerp(segment_start.fY, segment_end.fY, segment_eased) +
      direction_x / direction_length * curve_amount;

  const float shape_time = static_cast<float>(time_seconds) * 0.08F;
  const float stretch = 0.72F + HashUnit(blob_index + 43, cycle, seed) * 0.48F;
  const float rotation_direction = blob_index % 2 == 0 ? 1.0F : -1.0F;
  const float rotation_speed =
      0.018F + HashUnit(blob_index + 61, cycle, seed) * 0.018F;
  const float rotation =
      HashUnit(blob_index + 79, cycle, seed) * std::numbers::pi_v<float> *
          2.0F +
      static_cast<float>(time_seconds) * rotation_speed * rotation_direction;

  SkPath path;
  if (blob_index < kOctopusCount) {
    path = GenerateTentacledBlobPath(center_x, center_y, base_radius, rotation,
                                     stretch, shape_time, seed);
  } else if (blob_index < kOctopusCount + kHoledPolygonCount) {
    constexpr std::array<int, kHoledPolygonCount> kPolygonSides = {3, 4, 5, 6};
    const int polygon_index = blob_index - kOctopusCount;
    const float polygon_stretch =
        polygon_index == 1 ? stretch * 1.55F : stretch;
    path = GenerateHoledPolygonPath(kPolygonSides[polygon_index], center_x,
                                    center_y, base_radius * 1.45F, rotation,
                                    polygon_stretch);
  } else if (blob_index < kOctopusCount + kHoledPolygonCount + kPlusCount) {
    path = GeneratePlusPath(center_x, center_y, base_radius * 1.20F, rotation,
                            stretch);
  } else {
    const int circle_index =
        blob_index - kOctopusCount - kHoledPolygonCount - kPlusCount;
    const float circle_radius =
        base_radius * (1.0F + static_cast<float>(circle_index) * 0.22F);
    path = GenerateCirclePath(center_x, center_y, circle_radius);
  }

  return {std::move(path), static_cast<std::uint64_t>(blob_index)};
}

bool BoundsOverlap(const SkPath &first, const SkPath &second) {
  return SkRect::Intersects(first.getBounds(), second.getBounds());
}

bool IsVisibleOnCanvas(const SkPath &path, const SkRect &canvas_bounds) {
  return !path.isEmpty() && SkRect::Intersects(path.getBounds(), canvas_bounds);
}

std::vector<Region>
BuildDisjointRegions(const std::array<BlobShape, kBlobCount> &blobs,
                     const SkRect &canvas_bounds) {
  std::vector<Region> regions;
  for (const BlobShape &blob : blobs) {
    if (!IsVisibleOnCanvas(blob.path, canvas_bounds)) {
      continue;
    }

    std::vector<Region> next_regions;
    next_regions.reserve(regions.size() * 2 + 1);
    std::vector<SkPath> remainder = {blob.path};

    for (const Region &region : regions) {
      if (!BoundsOverlap(region.path, blob.path)) {
        next_regions.push_back(region);
        continue;
      }

      if (auto outside = Op(region.path, blob.path, kDifference_SkPathOp);
          outside.has_value() && IsVisibleOnCanvas(*outside, canvas_bounds)) {
        next_regions.push_back({std::move(*outside), region.id});
      }
      if (auto overlap = Op(region.path, blob.path, kIntersect_SkPathOp);
          overlap.has_value() && IsVisibleOnCanvas(*overlap, canvas_bounds)) {
        next_regions.push_back(
            {std::move(*overlap), CombinePieceIds(region.id, blob.id)});
      }

      std::vector<SkPath> next_remainder;
      for (const SkPath &remaining : remainder) {
        if (!BoundsOverlap(remaining, region.path)) {
          next_remainder.push_back(remaining);
          continue;
        }
        if (auto difference = Op(remaining, region.path, kDifference_SkPathOp);
            difference.has_value() &&
            IsVisibleOnCanvas(*difference, canvas_bounds)) {
          next_remainder.push_back(std::move(*difference));
        }
      }
      remainder = std::move(next_remainder);
    }

    for (SkPath &path : remainder) {
      if (IsVisibleOnCanvas(path, canvas_bounds)) {
        next_regions.push_back({std::move(path), blob.id});
      }
    }
    regions = std::move(next_regions);
  }
  return regions;
}

void DrawPieceCountChip(SkCanvas *canvas, int piece_count, float width,
                        float height, const sk_sp<SkTypeface> &typeface) {
  const float shortest = std::min(width, height);
  const float font_size = std::clamp(shortest * 0.032F, 18.0F, 28.0F);
  const float horizontal_padding = font_size * 0.65F;
  const float vertical_padding = font_size * 0.42F;
  const float margin = std::clamp(shortest * 0.025F, 12.0F, 22.0F);
  const std::string label = "#pieces: " + std::to_string(piece_count);
  const SkFont font(typeface, font_size);
  SkRect text_bounds;
  const float text_width = font.measureText(
      label.data(), label.size(), SkTextEncoding::kUTF8, &text_bounds);
  const float chip_width = text_width + horizontal_padding * 2.0F;
  const float chip_height = text_bounds.height() + vertical_padding * 2.0F;
  const SkRect chip = SkRect::MakeXYWH(width - margin - chip_width, margin,
                                       chip_width, chip_height);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SkColorSetRGB(28, 30, 29));
  canvas->drawRoundRect(chip, 6.0F, 6.0F, paint);

  paint.setColor(SK_ColorWHITE);
  const float baseline =
      chip.centerY() - (text_bounds.top() + text_bounds.bottom()) * 0.5F;
  canvas->drawSimpleText(label.data(), label.size(), SkTextEncoding::kUTF8,
                         chip.left() + horizontal_padding, baseline, font,
                         paint);
}

} // namespace

ShapeIntersectionFiddle::ShapeIntersectionFiddle() = default;

ShapeIntersectionFiddle::~ShapeIntersectionFiddle() = default;

bool ShapeIntersectionFiddle::UsesWebGl() const { return true; }

bool ShapeIntersectionFiddle::EnsureWebGl() {
  if (webgl_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;
  sk_sp<SkFontMgr> font_manager = SkiaFontManager::Instance().FontManager();
  if (font_manager != nullptr) {
    label_typeface_ =
        font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  }
  if (label_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Shape intersection could not resolve its "
                 "label typeface."
              << std::endl;
    return false;
  }

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(Canvas())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

void ShapeIntersectionFiddle::Render(double time_seconds) {
  if (!EnsureWebGl()) {
    return;
  }
  const int width = Canvas()["width"].as<int>();
  const int height = Canvas()["height"].as<int>();
  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }

  std::array<BlobShape, kBlobCount> blobs;
  for (int index = 0; index < kBlobCount; ++index) {
    blobs[index] = BuildBlob(index, time_seconds, static_cast<float>(width),
                             static_cast<float>(height));
  }
  const SkRect canvas_bounds =
      SkRect::MakeWH(static_cast<float>(width), static_cast<float>(height));
  const std::vector<Region> regions =
      BuildDisjointRegions(blobs, canvas_bounds);

  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kCanvasColor);
  SkPaint fill;
  fill.setAntiAlias(true);
  fill.setStyle(SkPaint::kFill_Style);
  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(2.0F);
  stroke.setStrokeCap(SkPaint::kRound_Cap);
  stroke.setStrokeJoin(SkPaint::kRound_Join);
  stroke.setBlendMode(SkBlendMode::kClear);

  for (const Region &region : regions) {
    SkColor4f color = ColorForPieceId(region.id);
    color.fA = 1.0F;
    fill.setColor4f(color, nullptr);
    canvas->drawPath(region.path, fill);
  }
  for (const Region &region : regions) {
    canvas->drawPath(region.path, stroke);
  }
  DrawPieceCountChip(canvas, static_cast<int>(regions.size()),
                     static_cast<float>(width), static_cast<float>(height),
                     label_typeface_);

  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Shape intersection could not submit its "
                 "WebGL frame."
              << std::endl;
  }
}
