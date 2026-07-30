#include <emscripten/bind.h>

#include "fiddle_manager.h"

EMSCRIPTEN_BINDINGS(CanvasWasmDemo) {
  emscripten::class_<FiddleManager>("FiddleManager")
      .constructor<emscripten::val, const std::string&>()
      .function("selectFiddle", &FiddleManager::SelectFiddle)
      .function("resize", &FiddleManager::Resize)
      .function("tick", &FiddleManager::Tick);
}
