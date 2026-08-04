#include <cstdint>
#include <string>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

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

bool LoadImage(const std::string &image_id,
               const emscripten::val &image_bytes) {
  const std::vector<std::uint8_t> bytes =
      emscripten::convertJSArrayToNumberVector<std::uint8_t>(image_bytes);
  return SkiaImageStore::Instance().RegisterEncodedImage(image_id, bytes.data(),
                                                         bytes.size());
}

} // namespace

EMSCRIPTEN_BINDINGS(CanvasWasmDemo) {
  emscripten::function("loadFont", &LoadFont);
  emscripten::function("loadImage", &LoadImage);
  emscripten::class_<WasmFiddleManager>("FiddleManager")
      .constructor<emscripten::val, const std::string &>()
      .function("selectFiddle", &WasmFiddleManager::SelectFiddle)
      .function("resize", &WasmFiddleManager::Resize)
      .function("tick", &WasmFiddleManager::Tick);
}
