#include "fiddles/sksl_shader_fiddle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>

#include "graphics/webgl_canvas_context.h"
#include "images/skia_image_store.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkImage.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkRuntimeEffect.h"
#include "text/skia_font_manager.h"

namespace {

constexpr double kPermutationSeconds = 0.5;
constexpr SkColor kBackgroundColor = SkColorSetRGB(165, 56, 96);

struct ChannelPermutation {
  std::array<int, 3> source_channels;
  const char *label;
};

constexpr std::array<ChannelPermutation, 5> kPermutations = {{
    {{0, 2, 1}, "R.G.B -> R.B.G"},
    {{1, 0, 2}, "R.G.B -> G.R.B"},
    {{1, 2, 0}, "R.G.B -> G.B.R"},
    {{2, 0, 1}, "R.G.B -> B.R.G"},
    {{2, 1, 0}, "R.G.B -> B.G.R"},
}};

void DrawLabel(SkCanvas *canvas, const std::string &text, float x, float y,
               const SkFont &font, SkColor color) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  canvas->drawSimpleText(text.data(), text.size(), SkTextEncoding::kUTF8, x, y,
                         font, paint);
}

void DrawImageBadge(SkCanvas *canvas, const char *text, float x, float top,
                    const SkFont &font) {
  const std::size_t length = std::char_traits<char>::length(text);
  const float font_size = font.getSize();
  const float width =
      font.measureText(text, length, SkTextEncoding::kUTF8) + font_size;
  SkPaint background;
  background.setColor(SkColorSetARGB(128, 0, 0, 0));
  canvas->drawRect(SkRect::MakeXYWH(x, top, width, font_size * 1.65F),
                   background);
  DrawLabel(canvas, text, x + font_size * 0.5F, top + font_size * 1.18F, font,
            SK_ColorWHITE);
}

} // namespace

SkslShaderFiddle::SkslShaderFiddle() = default;

SkslShaderFiddle::~SkslShaderFiddle() = default;

std::vector<FiddleWidget> SkslShaderFiddle::Widgets() const {
  return {{"image", "Input image", "image", image_id_}};
}

bool SkslShaderFiddle::SetInput(const std::string &name,
                                const std::string &value) {
  if (name != "image") {
    return false;
  }
  sk_sp<SkImage> image = SkiaImageStore::Instance().ImageForId(value);
  if (image == nullptr) {
    return false;
  }
  image_id_ = value;
  image_ = std::move(image);
  channel_shaders_.fill(nullptr);
  return BuildChannelShaders();
}

bool SkslShaderFiddle::EnsureResources() {
  if (webgl_ != nullptr && image_ != nullptr &&
      channel_shaders_[0] != nullptr) {
    return true;
  }
  if (initialization_attempted_) {
    return false;
  }
  initialization_attempted_ = true;

  image_ = SkiaImageStore::Instance().ImageForId(image_id_);
  if (image_ == nullptr) {
    std::cerr << "[cc-engine/stderr] SkSL shader requires worker "
                 "image id \""
              << image_id_ << "\"." << std::endl;
    return false;
  }

  sk_sp<SkFontMgr> font_manager = SkiaFontManager::Instance().FontManager();
  if (font_manager != nullptr) {
    label_typeface_ =
        font_manager->matchFamilyStyle("Roboto", SkFontStyle::Normal());
  }
  if (label_typeface_ == nullptr) {
    std::cerr << "[cc-engine/stderr] SkSL shader could not resolve "
                 "its label typeface."
              << std::endl;
    return false;
  }

  if (!BuildChannelShaders()) {
    return false;
  }

  auto webgl = std::make_unique<WebGlCanvasContext>();
  if (!webgl->Initialize(WebGlResource())) {
    return false;
  }
  webgl_ = std::move(webgl);

  std::cout << "[cc-engine/stdout] SkSL shader resources ready: "
               "five non-identity RGB permutations."
            << std::endl;
  return true;
}

bool SkslShaderFiddle::BuildChannelShaders() {
  const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
  sk_sp<SkShader> image_shader = image_->makeShader(sampling);
  if (image_shader == nullptr) {
    return false;
  }

  constexpr char kSksl[] = R"(
    uniform shader image;
    uniform float3 channel0;
    uniform float3 channel1;
    uniform float3 channel2;

    half4 main(float2 coord) {
      float4 color = image.eval(coord);
      return half4(dot(color.rgb, channel0),
                   dot(color.rgb, channel1),
                   dot(color.rgb, channel2),
                   color.a);
    }
  )";
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(kSksl));
  if (effect == nullptr) {
    std::cerr << "[cc-engine/stderr] SkSL channel shader failed: "
              << error.c_str() << std::endl;
    return false;
  }

  constexpr std::array<std::string_view, 3> kUniformNames = {
      "channel0", "channel1", "channel2"};
  for (std::size_t index = 0; index < kPermutations.size(); ++index) {
    SkRuntimeShaderBuilder builder(effect);
    builder.child("image") = image_shader;
    for (std::size_t output_channel = 0; output_channel < kUniformNames.size();
         ++output_channel) {
      std::array<float, 3> selector = {0.0F, 0.0F, 0.0F};
      selector[kPermutations[index].source_channels[output_channel]] = 1.0F;
      builder.uniform(kUniformNames[output_channel]) = selector;
    }
    channel_shaders_[index] = builder.makeShader();
    if (channel_shaders_[index] == nullptr) {
      return false;
    }
  }
  return true;
}

void SkslShaderFiddle::Render(double time_seconds) {
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
    std::cerr << "[cc-engine/stderr] SkSL shader could not submit "
                 "its WebGL frame."
              << std::endl;
  }
}

bool SkslShaderFiddle::UpdateState(double time_seconds, int, int) {
  time_seconds_ = time_seconds;
  return true;
}

void SkslShaderFiddle::DrawFrame(SkCanvas *canvas, int width, int height) {
  const float canvas_width = static_cast<float>(width);
  const float canvas_height = static_cast<float>(height);
  const float shortest = std::min(canvas_width, canvas_height);
  const float margin = std::clamp(shortest * 0.07F, 32.0F, 76.0F);
  const float gap = std::clamp(shortest * 0.055F, 28.0F, 58.0F);
  const float label_area = std::clamp(shortest * 0.11F, 62.0F, 104.0F);
  const float available_panel_height =
      std::max(1.0F, (canvas_height - margin * 2.0F - gap - label_area) * 0.5F);
  const float image_aspect = static_cast<float>(image_->width()) /
                             static_cast<float>(image_->height());
  const float panel_width = std::min(canvas_width - margin * 2.0F,
                                     available_panel_height * image_aspect);
  const float panel_height = panel_width / image_aspect;
  const float panel_left = (canvas_width - panel_width) * 0.5F;
  const float content_height = panel_height * 2.0F + gap;
  const float content_top =
      margin + std::max(0.0F, (canvas_height - margin * 2.0F - label_area -
                               content_height) *
                                  0.5F);
  const SkRect original_rect =
      SkRect::MakeXYWH(panel_left, content_top, panel_width, panel_height);
  const SkRect processed_rect = SkRect::MakeXYWH(
      panel_left, content_top + panel_height + gap, panel_width, panel_height);

  const std::size_t permutation_index =
      static_cast<std::size_t>(time_seconds_ / kPermutationSeconds) %
      kPermutations.size();

  canvas->clear(kBackgroundColor);

  SkPaint frame_paint;
  frame_paint.setAntiAlias(true);
  frame_paint.setStyle(SkPaint::kStroke_Style);
  frame_paint.setStrokeWidth(std::clamp(shortest * 0.006F, 3.0F, 6.0F));
  frame_paint.setColor(SK_ColorBLACK);

  const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);
  canvas->drawImageRect(image_, original_rect, sampling, nullptr);

  canvas->save();
  canvas->translate(processed_rect.left(), processed_rect.top());
  canvas->scale(processed_rect.width() / static_cast<float>(image_->width()),
                processed_rect.height() / static_cast<float>(image_->height()));
  SkPaint processed_paint;
  processed_paint.setShader(channel_shaders_[permutation_index]);
  canvas->drawRect(SkRect::MakeWH(static_cast<float>(image_->width()),
                                  static_cast<float>(image_->height())),
                   processed_paint);
  canvas->restore();
  canvas->drawRect(original_rect, frame_paint);
  canvas->drawRect(processed_rect, frame_paint);

  const float image_badge_size = std::clamp(shortest * 0.022F, 14.0F, 22.0F);
  SkFont badge_font(label_typeface_, image_badge_size);
  badge_font.setEdging(SkFont::Edging::kAntiAlias);
  const float badge_inset = std::max(2.0F, frame_paint.getStrokeWidth() * 0.5F);
  DrawImageBadge(canvas, "A  ORIGINAL", original_rect.left() + badge_inset,
                 original_rect.top() + badge_inset, badge_font);
  DrawImageBadge(canvas, "B  SKSL OUTPUT", processed_rect.left() + badge_inset,
                 processed_rect.top() + badge_inset, badge_font);

  const float mapping_font_size = std::clamp(shortest * 0.035F, 22.0F, 38.0F);
  SkFont mapping_font(label_typeface_, mapping_font_size);
  mapping_font.setEdging(SkFont::Edging::kAntiAlias);
  const std::string mapping = kPermutations[permutation_index].label;
  const float mapping_width = mapping_font.measureText(
      mapping.data(), mapping.size(), SkTextEncoding::kUTF8);
  const float mapping_y =
      std::min(canvas_height - margin * 0.45F,
               processed_rect.bottom() + gap + mapping_font_size * 1.35F);
  DrawLabel(canvas, mapping, (canvas_width - mapping_width) * 0.5F, mapping_y,
            mapping_font, SK_ColorWHITE);
}
