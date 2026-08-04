#include "core/fiddle_manager.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "fiddles/contour_lines_fiddle.h"
#include "fiddles/elastic_text_fiddle.h"
#include "fiddles/envelope_distort_fiddle.h"
#include "fiddles/ribbon_field_fiddle.h"
#include "fiddles/shape_intersection_fiddle.h"
#include "fiddles/shape_morphing_fiddle.h"
#include "fiddles/shape_tracing_fiddle.h"
#include "fiddles/skia_cpu_fiddle.h"
#include "fiddles/skia_webgl_fiddle.h"
#include "fiddles/sksl_image_proc_fiddle.h"
#include "fiddles/vortex_field_fiddle.h"

namespace {

std::unique_ptr<FiddleBase> CreateContourLines() {
  return std::make_unique<ContourLinesFiddle>();
}

std::unique_ptr<FiddleBase> CreateRibbonField() {
  return std::make_unique<RibbonFieldFiddle>();
}

std::unique_ptr<FiddleBase> CreateSkiaWebGl() {
  return std::make_unique<SkiaWebGlFiddle>();
}

std::unique_ptr<FiddleBase> CreateSkiaCpu() {
  return std::make_unique<SkiaCpuFiddle>();
}

std::unique_ptr<FiddleBase> CreateElasticText() {
  return std::make_unique<ElasticTextFiddle>();
}

std::unique_ptr<FiddleBase> CreateEnvelopeDistort() {
  return std::make_unique<EnvelopeDistortFiddle>();
}

std::unique_ptr<FiddleBase> CreateSkslImageProc() {
  return std::make_unique<SkslImageProcFiddle>();
}

std::unique_ptr<FiddleBase> CreateShapeIntersection() {
  return std::make_unique<ShapeIntersectionFiddle>();
}

std::unique_ptr<FiddleBase> CreateShapeTracing() {
  return std::make_unique<ShapeTracingFiddle>();
}

std::unique_ptr<FiddleBase> CreateShapeMorphing() {
  return std::make_unique<ShapeMorphingFiddle>();
}

std::unique_ptr<FiddleBase> CreateVortexField() {
  return std::make_unique<VortexFieldFiddle>();
}

} // namespace

FiddleManager::FiddleManager(FiddleCanvasResourceProvider &canvas_resources,
                             double initial_width, double initial_height,
                             const std::string &initial_key)
    : canvas_resources_(canvas_resources), width_(std::max(1.0, initial_width)),
      height_(std::max(1.0, initial_height)) {
  registry_.Register("contour-lines", &CreateContourLines);
  registry_.Register("ribbon-field", &CreateRibbonField);
  registry_.Register("skia-webgl", &CreateSkiaWebGl);
  registry_.Register("skia-cpu", &CreateSkiaCpu);
  registry_.Register("elastic-text", &CreateElasticText);
  registry_.Register("env-distort", &CreateEnvelopeDistort);
  registry_.Register("sksl-image-proc", &CreateSkslImageProc);
  registry_.Register("shape-intersection", &CreateShapeIntersection);
  registry_.Register("shape-tracing", &CreateShapeTracing);
  registry_.Register("shape-morphing", &CreateShapeMorphing);
  registry_.Register("vortex-field", &CreateVortexField);

  std::cout
      << "[cc-engine/stdout] FiddleManager initialized with "
      << "contour-lines, ribbon-field, skia-webgl, skia-cpu, elastic-text, "
         "env-distort, sksl-image-proc, shape-intersection, "
         "shape-tracing, shape-morphing, and vortex-field."
      << std::endl;
  std::cerr << "[cc-engine/stderr] C++ canvas error stream is connected."
            << std::endl;

  if (!SelectFiddle(initial_key)) {
    SelectFiddle("ribbon-field");
  }
}

bool FiddleManager::SelectFiddle(const std::string &key) {
  std::unique_ptr<FiddleBase> next_fiddle = registry_.Create(key);
  if (next_fiddle == nullptr) {
    std::cerr << "[cc-engine/stderr] Unknown fiddle key: " << key << std::endl;
    return false;
  }

  std::unique_ptr<FiddleCanvasResource> canvas =
      canvas_resources_.Create(next_fiddle->Backend());
  if (!next_fiddle->PopulateCanvas(std::move(canvas))) {
    std::cerr << "[cc-engine/stderr] Could not create the canvas backend for "
              << key << "." << std::endl;
    return false;
  }
  next_fiddle->Resize(width_, height_, device_pixel_ratio_);
  active_fiddle_ = std::move(next_fiddle);
  active_key_ = key;
  time_seconds_ = 0.0;

  std::cout << "[cc-engine/stdout] Activated fiddle: " << active_key_
            << std::endl;
  return true;
}

void FiddleManager::Resize(double width, double height,
                           double device_pixel_ratio) {
  width_ = std::max(1.0, width);
  height_ = std::max(1.0, height);
  device_pixel_ratio_ = std::max(1.0, device_pixel_ratio);
  if (active_fiddle_ != nullptr) {
    active_fiddle_->Resize(width_, height_, device_pixel_ratio_);
  }
}

void FiddleManager::Tick(double delta_seconds) {
  time_seconds_ += std::clamp(delta_seconds, 0.0, 0.1);
  if (active_fiddle_ != nullptr) {
    active_fiddle_->Render(time_seconds_);
  }
}
