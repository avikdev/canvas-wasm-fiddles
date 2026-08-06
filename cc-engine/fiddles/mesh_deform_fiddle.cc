#include "fiddles/mesh_deform_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <optional>
#include <random>
#include <string_view>

#include "geometry/shape_builder.h"
#include "graphics/canvas_legends.h"
#include "graphics/webgl_canvas_context.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "include/pathops/SkPathOps.h"
#include "text/skia_font_manager.h"
#include "utils/color_utils.h"

namespace {

constexpr SkColor kCanvasColor = 0xffd4d4d4;
constexpr float kLogicalOuterPadding = 16.0F;
constexpr float kLogicalPanelGap = 12.0F;
constexpr float kLogicalPanelInset = 7.0F;
constexpr float kLogicalBorderWidth = 1.5F;
constexpr float kLogicalPointerStrokeWidth = 4.0F;
constexpr float kLogicalPointerOutlineWidth = 2.0F;
constexpr float kLogicalArrowHeadLength = 8.0F;
constexpr float kLogicalGridStrokeWidth = 0.5F;
constexpr float kLogicalLegendBandHeight = 24.0F;
constexpr float kLogicalLegendWidth = 112.0F;
constexpr float kLogicalLegendHeight = 12.0F;

float RandomUnit(std::mt19937 *random) {
  return std::uniform_real_distribution<float>(0.0F, 1.0F)(*random);
}

SkPath MakeTriangle(const SkPoint &center, float radius, float rotation) {
  SkPathBuilder builder;
  for (int index = 0; index < 3; ++index) {
    const float angle = rotation + static_cast<float>(index) * 2.0F *
                                       std::numbers::pi_v<float> / 3.0F;
    const SkPoint point = {center.fX + std::cos(angle) * radius,
                           center.fY + std::sin(angle) * radius};
    if (index == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

SkPath MakePolygon(const SkPoint &center, float radius, int side_count,
                   float rotation) {
  SkPathBuilder builder;
  for (int index = 0; index < side_count; ++index) {
    const float angle = rotation + static_cast<float>(index) * 2.0F *
                                       std::numbers::pi_v<float> /
                                       static_cast<float>(side_count);
    const SkPoint point = {center.fX + std::cos(angle) * radius,
                           center.fY + std::sin(angle) * radius};
    if (index == 0) {
      builder.moveTo(point);
    } else {
      builder.lineTo(point);
    }
  }
  builder.close();
  return builder.detach();
}

float HueDistance(float first, float second) {
  const float direct = std::abs(first - second);
  return std::min(direct, 360.0F - direct);
}

SkPath MakeHatchLineRect(const SkPoint &center, float radius, float rotation) {
  SkPathBuilder builder;
  const float half_width = radius * 1.12F;
  const float half_height = radius * 0.72F;
  const float thickness = std::max(1.0F, radius * 0.11F);
  const float pitch = thickness * 2.1F;
  for (float y = -half_height; y < half_height; y += pitch) {
    const float bottom = std::min(y + thickness, half_height);
    builder.addRect(SkRect::MakeLTRB(-half_width, y, half_width, bottom));
  }
  SkPath result = builder.detach();
  SkMatrix placement;
  placement.setRotate(rotation * 180.0F / std::numbers::pi_v<float>);
  placement.postTranslate(center.fX, center.fY);
  result.transform(placement);
  return result;
}

SkPath MakeTextShape(std::string_view text, const sk_sp<SkTypeface> &typeface,
                     const SkPoint &center, float radius) {
  if (typeface == nullptr || text.empty()) {
    return {};
  }
  constexpr float source_size = 100.0F;
  const SkFont font(typeface, source_size);
  SkPathBuilder builder;
  float cursor = 0.0F;
  for (const char character : text) {
    const SkGlyphID glyph =
        font.unicharToGlyph(static_cast<unsigned char>(character));
    const std::optional<SkPath> glyph_path = font.getPath(glyph);
    if (glyph_path.has_value() && !glyph_path->isEmpty()) {
      SkPath positioned;
      glyph_path->transform(SkMatrix::Translate(cursor, 0.0F), &positioned);
      builder.addPath(positioned);
    }
    cursor += font.measureText(&character, 1U, SkTextEncoding::kUTF8);
  }
  SkPath result = builder.detach();
  const SkRect bounds = result.getBounds();
  if (bounds.isEmpty()) {
    return {};
  }
  const float scale = std::min(radius * 1.85F / bounds.width(),
                               radius * 1.60F / bounds.height());
  const SkMatrix placement = SkMatrix::ScaleTranslate(
      scale, scale, center.fX - bounds.centerX() * scale,
      center.fY - bounds.centerY() * scale);
  result.transform(placement);
  return result;
}

bool HoleIsInside(const SkPath &outer, const SkPath &hole) {
  SkPathMeasure measure(hole, false, 1.0F);
  do {
    constexpr int sample_count = 48;
    const float length = measure.getLength();
    for (int sample = 0; sample < sample_count; ++sample) {
      SkPoint point;
      if (!measure.getPosTan(length * static_cast<float>(sample) /
                                 static_cast<float>(sample_count),
                             &point, nullptr) ||
          !outer.contains(point.fX, point.fY)) {
        return false;
      }
    }
  } while (measure.nextContour());
  return true;
}

SkPath ComposeContainedHole(const SkPath &outer, const SkPath &hole,
                            const SkPoint &anchor) {
  if (hole.isEmpty() || !outer.contains(anchor.fX, anchor.fY)) {
    return outer;
  }
  SkPath candidate = hole;
  for (int attempt = 0; attempt < 14; ++attempt) {
    if (HoleIsInside(outer, candidate)) {
      const std::optional<SkPath> difference =
          Op(outer, candidate, kDifference_SkPathOp);
      return difference.has_value() ? *difference : outer;
    }
    SkMatrix shrink;
    shrink.setScale(0.82F, 0.82F, anchor.fX, anchor.fY);
    candidate.transform(shrink);
  }
  return outer;
}

std::vector<MeshDeformArtworkShape>
BuildSourceArtwork(int width, int height, std::uint32_t seed,
                   const sk_sp<SkTypeface> &typeface) {
  std::vector<MeshDeformArtworkShape> shapes;
  std::mt19937 random(seed);
  const int columns = std::clamp(width / 176, 4, 7);
  const int rows = std::clamp(height / 144, 2, 4);
  const float cell_width = static_cast<float>(width) / columns;
  const float cell_height = static_cast<float>(height) / rows;
  const float cell_size = std::min(cell_width, cell_height);
  shapes.reserve(columns * rows);
  std::vector<float> hues(static_cast<std::size_t>(columns * rows), -1.0F);
  constexpr float minimum_adjacent_hue_distance = 52.0F;
  constexpr std::array<std::string_view, 11> labels = {
      "Ah", "Hi", "@", "A", "B", "C", "D", "R", "S", "M", "W"};

  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const float jitter_x = (RandomUnit(&random) - 0.5F) * cell_width * 0.48F;
      const float jitter_y = (RandomUnit(&random) - 0.5F) * cell_height * 0.48F;
      const SkPoint center = {
          (static_cast<float>(column) + 0.5F) * cell_width + jitter_x,
          (static_cast<float>(row) + 0.5F) * cell_height + jitter_y};
      const float radius = cell_size * (0.57F + RandomUnit(&random) * 0.25F);
      const float rotation =
          RandomUnit(&random) * 2.0F * std::numbers::pi_v<float>;
      const int kind = static_cast<int>(random() % 11U);

      MeshDeformArtworkShape shape;
      float hue = RandomUnit(&random) * 360.0F;
      const auto is_distinct_from_neighbors = [&](float candidate) {
        const bool differs_from_left =
            column == 0 ||
            HueDistance(
                candidate,
                hues[static_cast<std::size_t>(row * columns + column - 1)]) >=
                minimum_adjacent_hue_distance;
        const bool differs_from_top =
            row == 0 ||
            HueDistance(
                candidate,
                hues[static_cast<std::size_t>((row - 1) * columns + column)]) >=
                minimum_adjacent_hue_distance;
        return differs_from_left && differs_from_top;
      };
      for (int attempt = 0; attempt < 16; ++attempt) {
        if (is_distinct_from_neighbors(hue)) {
          break;
        }
        hue = RandomUnit(&random) * 360.0F;
      }
      if (!is_distinct_from_neighbors(hue)) {
        for (int degree = 0; degree < 360; ++degree) {
          const float fallback_hue = static_cast<float>(degree);
          if (is_distinct_from_neighbors(fallback_hue)) {
            hue = fallback_hue;
            break;
          }
        }
      }
      hues[static_cast<std::size_t>(row * columns + column)] = hue;
      const float saturation = 0.76F + RandomUnit(&random) * 0.12F;
      const float lightness = 0.48F + RandomUnit(&random) * 0.18F;
      shape.color = color_utils::FromHsl(hue, saturation, lightness);

      switch (kind) {
      case 0: {
        SkPathBuilder outer_builder;
        outer_builder.addCircle(center.fX, center.fY, radius);
        shape.path = outer_builder.detach();
        SkPathBuilder hole_builder;
        hole_builder.addCircle(center.fX, center.fY, radius * 0.58F);
        shape.hole = hole_builder.detach();
        shape.hole_center = center;
        break;
      }
      case 1:
        shape.path = MakeTriangle(center, radius, rotation);
        break;
      case 2:
        shape.path =
            geometry::shapes::MakeStar(center, radius, 6, rotation, 0.52F);
        break;
      case 3:
        shape.path =
            geometry::shapes::MakeCross(center, radius, rotation, 0.25F);
        break;
      case 4:
        shape.path = geometry::shapes::MakeTentacledBlob(
            center, radius, 4 + static_cast<int>(random() % 3U), rotation,
            random());
        break;
      case 5:
        shape.path = MakePolygon(center, radius,
                                 5 + static_cast<int>(random() % 3U), rotation);
        shape.hole_center = center;
        {
          SkPathBuilder hole_builder;
          hole_builder.addCircle(center.fX, center.fY, radius * 0.34F);
          shape.hole = hole_builder.detach();
        }
        break;
      case 6: {
        shape.path = MakePolygon(center, radius, 6, rotation);
        shape.hole_center = center;
        shape.hole = MakeTriangle(center, radius * 0.32F, rotation + 0.35F);
        break;
      }
      case 7: {
        const SkRect oblong = SkRect::MakeXYWH(center.fX - radius * 1.13F,
                                               center.fY - radius * 0.30F,
                                               radius * 2.26F, radius * 0.60F);
        SkPathBuilder builder;
        builder.addRRect(
            SkRRect::MakeRectXY(oblong, radius * 0.12F, radius * 0.12F));
        shape.path = builder.detach();
        SkMatrix matrix;
        matrix.setRotate(rotation * 180.0F / std::numbers::pi_v<float>,
                         center.fX, center.fY);
        shape.path.transform(matrix);
        shape.stroke_width = std::max(3.0F, radius * 0.13F);
        break;
      }
      case 8:
      case 9:
        shape.path = MakeTextShape(
            labels[static_cast<std::size_t>(random() % labels.size())],
            typeface, center, radius);
        break;
      default:
        shape.path = MakeHatchLineRect(center, radius, rotation);
        break;
      }
      shapes.push_back(std::move(shape));
    }
  }
  return shapes;
}

void DrawArrow(SkCanvas *canvas, const SkPoint &start, const SkPoint &end,
               const SkPaint &paint, float head_length) {
  canvas->drawLine(start, end, paint);
  const float dx = end.fX - start.fX;
  const float dy = end.fY - start.fY;
  const float length = std::hypot(dx, dy);
  if (length <= 0.001F) {
    return;
  }
  const float unit_x = dx / length;
  const float unit_y = dy / length;
  constexpr float spread = 0.48F;
  const SkPoint left = {end.fX - head_length * (unit_x * std::cos(spread) -
                                                unit_y * std::sin(spread)),
                        end.fY - head_length * (unit_y * std::cos(spread) +
                                                unit_x * std::sin(spread))};
  const SkPoint right = {end.fX - head_length * (unit_x * std::cos(spread) +
                                                 unit_y * std::sin(spread)),
                         end.fY - head_length * (unit_y * std::cos(spread) -
                                                 unit_x * std::sin(spread))};
  canvas->drawLine(end, left, paint);
  canvas->drawLine(end, right, paint);
}

} // namespace

MeshDeformFiddle::MeshDeformFiddle()
    : MeshDeformFiddle(MeshDeformFiddleOptions{}) {}
MeshDeformFiddle::MeshDeformFiddle(const MeshDeformFiddleOptions &options)
    : options_(options) {}
MeshDeformFiddle::~MeshDeformFiddle() = default;

bool MeshDeformFiddle::EnsureResources() {
  if (webgl_ != nullptr && artwork_typeface_ != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;
  const sk_sp<SkFontMgr> font_manager =
      SkiaFontManager::Instance().FontManager();
  if (font_manager == nullptr || font_manager->countFamilies() == 0) {
    std::cerr << "[cc-engine/stderr] Mesh deform could not access the shared "
                 "font manager."
              << std::endl;
    return false;
  }
  artwork_typeface_ =
      font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  if (artwork_typeface_ == nullptr) {
    SkString family;
    font_manager->getFamilyName(0, &family);
    artwork_typeface_ =
        font_manager->matchFamilyStyle(family.c_str(), SkFontStyle::Normal());
  }
  if (artwork_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] Mesh deform could not resolve an artwork "
                 "typeface."
              << std::endl;
    return false;
  }
  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);
  return true;
}

void MeshDeformFiddle::Render(double time_seconds) {
  if (!EnsureResources()) {
    return;
  }
  const int width = PixelWidth();
  const int height = PixelHeight();
  if ((cached_width_ != width || cached_height_ != height) &&
      !RebuildScene(width, height)) {
    return;
  }
  AdvanceSyntheticDrag(time_seconds);

  SkSurface *surface = webgl_->AcquireSurface(width, height);
  if (surface == nullptr) {
    return;
  }
  DrawFrame(surface->getCanvas(), width, height);
  const WebGlPresentResult present = webgl_->FlushAndPresent();
  if (!present.success) {
    std::cerr << "[cc-engine/stderr] Mesh deform could not submit its WebGL "
                 "frame."
              << std::endl;
  }
}

bool MeshDeformFiddle::RebuildScene(int width, int height) {
  device_scale_ =
      static_cast<float>(width / std::max(1.0, static_cast<double>(Width())));
  const float padding =
      std::min(kLogicalOuterPadding * device_scale_,
               std::max(3.0F, std::min(width, height) * 0.08F));
  const float gap = kLogicalPanelGap * device_scale_;
  const float inset = kLogicalPanelInset * device_scale_;
  const float legend_band = kLogicalLegendBandHeight * device_scale_;
  source_width_ = std::floor(std::max(1.0F, static_cast<float>(width) -
                                                2.0F * padding - 2.0F * inset));
  source_height_ = std::floor(std::max(
      1.0F,
      (static_cast<float>(height) - 2.0F * padding - legend_band - gap) * 0.5F -
          2.0F * inset));
  const int image_width = std::max(1, static_cast<int>(source_width_));
  const int image_height = std::max(1, static_cast<int>(source_height_));
  const std::uint32_t artwork_seed =
      0x4d455348U + scene_generation_ * 0x9e3779b9U;
  shapes_ = BuildSourceArtwork(image_width, image_height, artwork_seed,
                               artwork_typeface_);

  graphics::MeshDeformerOptions mesh_options;
  mesh_options.epsilon_gap =
      std::max(2.5F, options_.epsilon_gap * device_scale_);
  mesh_options.push_strength = options_.push_strength;
  const float maximum_drag_distance = std::max(source_width_, source_height_) *
                                      options_.maximum_drag_distance_ratio;
  mesh_options.maximum_drag_distance = maximum_drag_distance;
  mesh_options.damping_start_distance =
      maximum_drag_distance * options_.damping_start_distance_ratio;
  mesh_options.far_drag_response = options_.far_drag_response;
  mesh_options.minimum_cell_area_ratio = options_.minimum_cell_area_ratio;
  if (!deformer_.Build(SkRect::MakeWH(source_width_, source_height_),
                       mesh_options)) {
    return false;
  }

  const float panel_height =
      (static_cast<float>(height) - 2.0F * padding - legend_band - gap) * 0.5F;
  const float top_panel_y = padding + legend_band;
  output_origin_ = {padding + inset, top_panel_y + panel_height + gap + inset};
  input::FakeMouseActionsOptions action_options;
  action_options.edge_inset =
      std::min(source_width_, source_height_) * options_.edge_inset_ratio;
  action_options.maximum_drag_distance = maximum_drag_distance;
  action_options.inward_drag_weight = options_.inward_drag_weight;
  action_options.outward_drag_weight = options_.outward_drag_weight;
  action_options.center_drag_weight = options_.center_drag_weight;
  action_options.sideways_drag_weight = options_.sideways_drag_weight;
  action_options.center_region_ratio = options_.center_region_ratio;
  action_options.minimum_drag_length_ratio = options_.minimum_drag_length_ratio;
  action_options.idle_frames_between_drags = options_.idle_frames_between_drags;
  action_options.seed = 0x4d455348U + scene_generation_ * 7919U;
  fake_mouse_ = std::make_unique<input::FakeMouseActions>(
      SkRect::MakeWH(source_width_, source_height_), action_options);
  completed_drag_count_ = 0U;
  active_frame_ = {};
  deformer_drag_active_ = false;
  reset_pending_ = false;
  reset_started_at_ = 0.0;
  cycle_remaining_fraction_ = 1.0F;
  cached_width_ = width;
  cached_height_ = height;
  return true;
}

void MeshDeformFiddle::AdvanceSyntheticDrag(double time_seconds) {
  const std::size_t drag_count_before_reset =
      std::max<std::size_t>(1U, options_.drag_count_before_reset);
  const double reset_delay_seconds =
      std::max(0.001, options_.reset_delay_seconds);
  const float cycle_unit_count =
      static_cast<float>(drag_count_before_reset) + 1.0F;
  if (reset_pending_) {
    const double elapsed = std::isfinite(time_seconds)
                               ? std::max(0.0, time_seconds - reset_started_at_)
                               : reset_delay_seconds;
    const float hold_remaining = static_cast<float>(
        std::clamp(1.0 - elapsed / reset_delay_seconds, 0.0, 1.0));
    cycle_remaining_fraction_ = hold_remaining / cycle_unit_count;
    if (elapsed >= reset_delay_seconds) {
      ++scene_generation_;
      RebuildScene(cached_width_, cached_height_);
    }
    return;
  }
  if (fake_mouse_ == nullptr) {
    return;
  }
  active_frame_ = fake_mouse_->Advance(options_.pointer_step * device_scale_);
  float active_drag_progress = 0.0F;
  if (active_frame_.in_drag) {
    const float total_distance =
        std::hypot(active_frame_.end.fX - active_frame_.start.fX,
                   active_frame_.end.fY - active_frame_.start.fY);
    if (total_distance > 0.001F) {
      active_drag_progress = std::clamp(
          std::hypot(active_frame_.current.fX - active_frame_.start.fX,
                     active_frame_.current.fY - active_frame_.start.fY) /
              total_distance,
          0.0F, 1.0F);
    }
  }
  cycle_remaining_fraction_ =
      std::clamp(1.0F - (static_cast<float>(completed_drag_count_) +
                         active_drag_progress) /
                            cycle_unit_count,
                 0.0F, 1.0F);
  if (!active_frame_.in_drag) {
    return;
  }
  if (active_frame_.began) {
    const float radius =
        std::min(source_width_, source_height_) * options_.brush_radius_ratio;
    deformer_drag_active_ = deformer_.BeginDrag(active_frame_.start, radius);
  }
  if (deformer_drag_active_) {
    deformer_.UpdateDrag(active_frame_.current, options_.drag_intensity);
  }
  if (active_frame_.ended && deformer_drag_active_) {
    deformer_.CommitDrag();
    ++completed_drag_count_;
    if (completed_drag_count_ >= drag_count_before_reset) {
      reset_pending_ = true;
      reset_started_at_ = time_seconds;
      cycle_remaining_fraction_ = 1.0F / cycle_unit_count;
    }
    deformer_drag_active_ = false;
    active_frame_ = {};
  }
}

void MeshDeformFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  canvas->clear(kCanvasColor);
  if (shapes_.empty()) {
    return;
  }
  const float padding =
      std::min(kLogicalOuterPadding * device_scale_,
               std::max(3.0F, std::min(width, height) * 0.08F));
  const float gap = kLogicalPanelGap * device_scale_;
  const float legend_band = kLogicalLegendBandHeight * device_scale_;
  const float panel_height =
      (static_cast<float>(height) - 2.0F * padding - legend_band - gap) * 0.5F;
  const float inset = kLogicalPanelInset * device_scale_;
  const float top_panel_y = padding + legend_band;
  const SkRect top_panel = SkRect::MakeXYWH(
      padding, top_panel_y, width - 2.0F * padding, panel_height);
  const SkRect bottom_panel =
      SkRect::MakeXYWH(padding, top_panel_y + panel_height + gap,
                       width - 2.0F * padding, panel_height);

  SkPaint panel_paint;
  panel_paint.setColor(SK_ColorWHITE);
  panel_paint.setStyle(SkPaint::kFill_Style);
  canvas->drawRect(top_panel, panel_paint);
  canvas->drawRect(bottom_panel, panel_paint);

  SkPaint shape_paint;
  shape_paint.setAntiAlias(true);
  canvas->save();
  canvas->translate(padding + inset, top_panel_y + inset);
  canvas->clipRect(SkRect::MakeWH(source_width_, source_height_),
                   SkClipOp::kIntersect, true);
  for (const MeshDeformArtworkShape &shape : shapes_) {
    const SkPath drawable =
        ComposeContainedHole(shape.path, shape.hole, shape.hole_center);
    shape_paint.setColor4f(shape.color);
    shape_paint.setStyle(shape.stroke_width > 0.0F ? SkPaint::kStroke_Style
                                                   : SkPaint::kFill_Style);
    shape_paint.setStrokeWidth(shape.stroke_width);
    shape_paint.setStrokeJoin(SkPaint::kRound_Join);
    canvas->drawPath(drawable, shape_paint);
  }
  canvas->restore();

  canvas->save();
  canvas->translate(output_origin_.fX, output_origin_.fY);
  canvas->clipRect(SkRect::MakeWH(source_width_, source_height_),
                   SkClipOp::kIntersect, true);
  for (const MeshDeformArtworkShape &shape : shapes_) {
    const SkPath deformed_outer = deformer_.DeformPath(shape.path);
    const SkPath deformed_hole = deformer_.DeformPath(shape.hole);
    const SkPoint deformed_anchor = deformer_.MapPoint(shape.hole_center);
    const SkPath deformed =
        ComposeContainedHole(deformed_outer, deformed_hole, deformed_anchor);
    shape_paint.setColor4f(shape.color);
    shape_paint.setStyle(shape.stroke_width > 0.0F ? SkPaint::kStroke_Style
                                                   : SkPaint::kFill_Style);
    shape_paint.setStrokeWidth(shape.stroke_width);
    shape_paint.setStrokeJoin(SkPaint::kRound_Join);
    canvas->drawPath(deformed, shape_paint);
  }

  if (!reset_pending_) {
    SkPaint grid_paint;
    grid_paint.setAntiAlias(true);
    grid_paint.setColor(0x33000000);
    grid_paint.setStyle(SkPaint::kStroke_Style);
    grid_paint.setStrokeWidth(kLogicalGridStrokeWidth * device_scale_);
    grid_paint.setStrokeCap(SkPaint::kRound_Cap);
    grid_paint.setStrokeJoin(SkPaint::kRound_Join);
    canvas->drawPath(deformer_.ControlGridPath(), grid_paint);
  }

  if (active_frame_.in_drag) {
    SkPaint arrow_outline_paint;
    arrow_outline_paint.setAntiAlias(true);
    arrow_outline_paint.setColor(SK_ColorWHITE);
    arrow_outline_paint.setStyle(SkPaint::kStroke_Style);
    arrow_outline_paint.setStrokeWidth(
        (kLogicalPointerStrokeWidth + kLogicalPointerOutlineWidth) *
        device_scale_);
    arrow_outline_paint.setStrokeCap(SkPaint::kRound_Cap);
    arrow_outline_paint.setStrokeJoin(SkPaint::kRound_Join);
    DrawArrow(canvas, active_frame_.start, active_frame_.current,
              arrow_outline_paint, kLogicalArrowHeadLength * device_scale_);

    SkPaint arrow_paint = arrow_outline_paint;
    arrow_paint.setColor(SK_ColorBLACK);
    arrow_paint.setStrokeWidth(kLogicalPointerStrokeWidth * device_scale_);
    DrawArrow(canvas, active_frame_.start, active_frame_.current, arrow_paint,
              kLogicalArrowHeadLength * device_scale_);
  }
  canvas->restore();

  SkPaint border_paint;
  border_paint.setAntiAlias(true);
  border_paint.setColor(SK_ColorBLACK);
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setStrokeWidth(kLogicalBorderWidth * device_scale_);
  canvas->drawRect(top_panel, border_paint);
  canvas->drawRect(bottom_panel, border_paint);

  const float legend_width = kLogicalLegendWidth * device_scale_;
  const float legend_height = kLogicalLegendHeight * device_scale_;
  const SkRect legend_bounds =
      SkRect::MakeXYWH(static_cast<float>(width) - padding - legend_width,
                       padding + (legend_band - legend_height) * 0.5F,
                       legend_width, legend_height);
  graphics::canvas_legends::ProgressChipStyle legend_style;
  legend_style.border_width = 1.0F * device_scale_;
  legend_style.padding = 2.0F * device_scale_;
  legend_style.corner_radius = 6.0F * device_scale_;
  graphics::canvas_legends::DrawProgressChip(
      canvas, legend_bounds, cycle_remaining_fraction_, legend_style);
}
