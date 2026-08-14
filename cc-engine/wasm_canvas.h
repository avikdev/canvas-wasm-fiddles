#pragma once

#include <memory>
#include <string>
#include <vector>

#include <emscripten/val.h>

class FiddleManager;
struct FiddleWidget;

// Emscripten-facing wrapper around the platform-neutral fiddle manager. This
// is deliberately kept at the executable boundary so fiddle code can compile
// without Emscripten headers.
class WasmFiddleManager {
public:
  WasmFiddleManager(emscripten::val canvas, const std::string &initial_key);
  ~WasmFiddleManager();

  WasmFiddleManager(const WasmFiddleManager &) = delete;
  WasmFiddleManager &operator=(const WasmFiddleManager &) = delete;

  bool SelectFiddle(const std::string &key);
  bool IsSvgWritable() const;
  std::vector<FiddleWidget> Widgets() const;
  bool SetInput(const std::string &name, const std::string &value);
  std::string ExportSvg();
  void Resize(double width, double height, double device_pixel_ratio);
  void Tick(double delta_seconds);

private:
  class ResourceProvider;

  std::unique_ptr<ResourceProvider> canvas_resources_;
  std::unique_ptr<FiddleManager> manager_;
};
