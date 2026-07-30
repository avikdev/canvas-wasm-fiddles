#pragma once

#include <memory>
#include <string_view>

#include <emscripten/val.h>

#include "include/core/SkRefCnt.h"

class SkSurface;

namespace skgpu::graphite {
class Context;
class Recorder;
}  // namespace skgpu::graphite

class GraphiteCanvasContext final {
 public:
  GraphiteCanvasContext();
  ~GraphiteCanvasContext();

  GraphiteCanvasContext(const GraphiteCanvasContext&) = delete;
  GraphiteCanvasContext& operator=(const GraphiteCanvasContext&) = delete;

  bool Initialize();
  sk_sp<SkSurface> AcquireSurface(const emscripten::val& canvas_context,
                                  std::string_view texture_format);
  bool Submit();

 private:
  std::unique_ptr<skgpu::graphite::Context> context_;
  std::unique_ptr<skgpu::graphite::Recorder> recorder_;
};
