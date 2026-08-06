#include "core/fiddle_base.h"

#include <memory>

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkRect.h"
#include "include/core/SkStream.h"
#include "include/svg/SkSVGCanvas.h"

std::string FiddleBase::ExportSvg() {
  if (!IsSvgWritable()) {
    return {};
  }

  SkDynamicMemoryWStream stream;
  SkSVGCanvas::Options options;
  options.flags = SkSVGCanvas::kConvertTextToPaths_Flag;
  const int width = PixelWidth();
  const int height = PixelHeight();
  std::unique_ptr<SkCanvas> svg_canvas =
      SkSVGCanvas::Make(SkRect::MakeIWH(width, height), &stream, options);
  if (svg_canvas == nullptr) {
    return {};
  }
  DrawFrame(svg_canvas.get(), width, height);
  svg_canvas.reset();

  sk_sp<SkData> svg_data = stream.detachAsData();
  if (svg_data == nullptr || svg_data->isEmpty()) {
    return {};
  }
  return {static_cast<const char *>(svg_data->data()), svg_data->size()};
}
