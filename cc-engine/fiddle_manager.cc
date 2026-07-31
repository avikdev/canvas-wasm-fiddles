#include "fiddle_manager.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>

#include "fiddles/elastic_text_fiddle.h"
#include "fiddles/orbital_bloom_fiddle.h"
#include "fiddles/ribbon_field_fiddle.h"
#include "fiddles/shape_intersection_fiddle.h"
#include "fiddles/skia_cpu_fiddle.h"
#include "fiddles/skia_webgl_fiddle.h"
#include "fiddles/sksl_image_proc_fiddle.h"

namespace {

std::unique_ptr<FiddleBase> CreateOrbitalBloom() {
  return std::make_unique<OrbitalBloomFiddle>();
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

std::unique_ptr<FiddleBase> CreateSkslImageProc() {
  return std::make_unique<SkslImageProcFiddle>();
}

std::unique_ptr<FiddleBase> CreateShapeIntersection() {
  return std::make_unique<ShapeIntersectionFiddle>();
}

} // namespace

FiddleManager::FiddleManager(emscripten::val canvas,
                             const std::string &initial_key)
    : canvas_(std::move(canvas)) {
  registry_.Register("orbital-bloom", &CreateOrbitalBloom);
  registry_.Register("ribbon-field", &CreateRibbonField);
  registry_.Register("skia-webgl", &CreateSkiaWebGl);
  registry_.Register("skia-cpu", &CreateSkiaCpu);
  registry_.Register("elastic-text", &CreateElasticText);
  registry_.Register("sksl-image-proc", &CreateSkslImageProc);
  registry_.Register("shape-intersection", &CreateShapeIntersection);

  width_ = canvas_["width"].as<double>();
  height_ = canvas_["height"].as<double>();

  std::cout << "[cc-engine/stdout] FiddleManager initialized with "
            << "orbital-bloom, ribbon-field, skia-webgl, skia-cpu, "
               "elastic-text, sksl-image-proc, and shape-intersection."
            << std::endl;
  std::cerr << "[cc-engine/stderr] C++ canvas error stream is connected."
            << std::endl;

  if (!SelectFiddle(initial_key)) {
    SelectFiddle("orbital-bloom");
  }
}

bool FiddleManager::SelectFiddle(const std::string &key) {
  std::unique_ptr<FiddleBase> next_fiddle = registry_.Create(key);
  if (next_fiddle == nullptr) {
    std::cerr << "[cc-engine/stderr] Unknown fiddle key: " << key << std::endl;
    return false;
  }

  next_fiddle->PopulateCanvas(canvas_);
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
