#include <cstdint>
#include <string>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "fiddle_manager.h"
#include "text/skia_font_manager.h"

namespace {

bool LoadFont(const std::string &font_id, const emscripten::val &font_bytes) {
  const std::vector<std::uint8_t> bytes =
      emscripten::convertJSArrayToNumberVector<std::uint8_t>(font_bytes);
  return SkiaFontManager::Instance().RegisterFont(font_id, bytes.data(),
                                                  bytes.size());
}

} // namespace

EMSCRIPTEN_BINDINGS(CanvasWasmDemo) {
  emscripten::function("loadFont", &LoadFont);
  emscripten::class_<FiddleManager>("FiddleManager")
      .constructor<emscripten::val, const std::string &>()
      .function("selectFiddle", &FiddleManager::SelectFiddle)
      .function("resize", &FiddleManager::Resize)
      .function("tick", &FiddleManager::Tick);
}
