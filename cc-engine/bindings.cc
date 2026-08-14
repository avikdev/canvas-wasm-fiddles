#include <cstdint>
#include <string>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "core/fiddle_base.h"
#include "images/skia_image_store.h"
#include "text/skia_font_manager.h"
#include "wasm_canvas.h"

namespace {

bool LoadFont(const std::string &font_id, const emscripten::val &font_bytes) {
  const std::vector<std::uint8_t> bytes =
      emscripten::convertJSArrayToNumberVector<std::uint8_t>(font_bytes);
  return SkiaFontManager::Instance().RegisterFont(font_id, bytes.data(),
                                                  bytes.size());
}

bool LoadImageBitmap(const std::string &image_id,
                     const emscripten::val &bitmap) {
  if (bitmap.isNull() || bitmap.isUndefined()) {
    return false;
  }
  const int width = bitmap["width"].as<int>();
  const int height = bitmap["height"].as<int>();
  if (width <= 0 || height <= 0) {
    return false;
  }
  emscripten::val canvas =
      emscripten::val::global("OffscreenCanvas").new_(width, height);
  emscripten::val context =
      canvas.call<emscripten::val>("getContext", std::string("2d"));
  context.call<void>("drawImage", bitmap, 0, 0);
  emscripten::val pixels = context.call<emscripten::val>("getImageData", 0, 0,
                                                         width, height)["data"];
  const std::vector<std::uint8_t> rgba =
      emscripten::convertJSArrayToNumberVector<std::uint8_t>(pixels);
  return SkiaImageStore::Instance().RegisterRgbaImage(image_id, rgba.data(),
                                                      width, height);
}

} // namespace

EMSCRIPTEN_BINDINGS(CanvasWasmDemo) {
  emscripten::register_vector<std::string>("StringVector");
  emscripten::register_vector<FiddleWidget>("FiddleWidgetVector");
  emscripten::value_object<FiddleWidget>("FiddleWidget")
      .field("key", &FiddleWidget::key)
      .field("title", &FiddleWidget::title)
      .field("type", &FiddleWidget::type)
      .field("defaultValue", &FiddleWidget::default_value)
      .field("options", &FiddleWidget::options)
      .field("min", &FiddleWidget::minimum)
      .field("max", &FiddleWidget::maximum)
      .field("step", &FiddleWidget::step);
  emscripten::function("loadFont", &LoadFont);
  emscripten::function("loadImageBitmap", &LoadImageBitmap);
  emscripten::class_<WasmFiddleManager>("FiddleManager")
      .constructor<emscripten::val, const std::string &>()
      .function("selectFiddle", &WasmFiddleManager::SelectFiddle)
      .function("isSvgWritable", &WasmFiddleManager::IsSvgWritable)
      .function("widgets", &WasmFiddleManager::Widgets)
      .function("setInput", &WasmFiddleManager::SetInput)
      .function("exportSvg", &WasmFiddleManager::ExportSvg)
      .function("resize", &WasmFiddleManager::Resize)
      .function("tick", &WasmFiddleManager::Tick);
}
