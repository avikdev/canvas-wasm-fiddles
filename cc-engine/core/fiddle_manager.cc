#include "core/fiddle_manager.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "fiddles/contour_composite_fiddle.h"
#include "fiddles/contour_lines_fiddle.h"
#include "fiddles/curve_interpolate_fiddle.h"
#include "fiddles/envelope_distort_fiddle.h"
#include "fiddles/mesh_warp_fiddle.h"
#include "fiddles/noise_deform_fiddle.h"
#include "fiddles/pucker_bloat_fiddle.h"
#include "fiddles/scene_benchmark_cpu_fiddle.h"
#include "fiddles/scene_benchmark_webgl_fiddle.h"
#include "fiddles/shape_intersection_fiddle.h"
#include "fiddles/sksl_shader_fiddle.h"
#include "fiddles/swirl_deform_fiddle.h"
#include "fiddles/text_cutting_fiddle.h"
#include "fiddles/text_morphing_fiddle.h"
#include "fiddles/text_reflow_fiddle.h"
#include "fiddles/text_tracing_fiddle.h"

namespace {

std::unique_ptr<FiddleBase> CreateContourLines() {
  return std::make_unique<ContourLinesFiddle>();
}

std::unique_ptr<FiddleBase> CreateContourComposite() {
  return std::make_unique<ContourCompositeFiddle>();
}

std::unique_ptr<FiddleBase> CreateCurveInterpolate() {
  return std::make_unique<CurveInterpolateFiddle>();
}

std::unique_ptr<FiddleBase> CreateTextCutting() {
  return std::make_unique<TextCuttingFiddle>();
}

std::unique_ptr<FiddleBase> CreateSceneBenchmarkWebGl() {
  return std::make_unique<SceneBenchmarkWebGlFiddle>();
}

std::unique_ptr<FiddleBase> CreateSceneBenchmarkCpu() {
  return std::make_unique<SceneBenchmarkCpuFiddle>();
}

std::unique_ptr<FiddleBase> CreateTextReflow() {
  return std::make_unique<TextReflowFiddle>();
}

std::unique_ptr<FiddleBase> CreateEnvelopeDistort() {
  return std::make_unique<EnvelopeDistortFiddle>();
}

std::unique_ptr<FiddleBase> CreateMeshWarp() {
  return std::make_unique<MeshWarpFiddle>();
}

std::unique_ptr<FiddleBase> CreatePuckerBloat() {
  return std::make_unique<PuckerBloatFiddle>();
}

std::unique_ptr<FiddleBase> CreateSkslShader() {
  return std::make_unique<SkslShaderFiddle>();
}

std::unique_ptr<FiddleBase> CreateShapeIntersection() {
  return std::make_unique<ShapeIntersectionFiddle>();
}

std::unique_ptr<FiddleBase> CreateTextTracing() {
  return std::make_unique<TextTracingFiddle>();
}

std::unique_ptr<FiddleBase> CreateTextMorphing() {
  return std::make_unique<TextMorphingFiddle>();
}

std::unique_ptr<FiddleBase> CreateSwirlDeform() {
  return std::make_unique<SwirlDeformFiddle>();
}

std::unique_ptr<FiddleBase> CreateNoiseDeform() {
  return std::make_unique<NoiseDeformFiddle>();
}

} // namespace

FiddleManager::FiddleManager(FiddleCanvasResourceProvider &canvas_resources,
                             double initial_width, double initial_height,
                             const std::string &initial_key)
    : canvas_resources_(canvas_resources), width_(std::max(1.0, initial_width)),
      height_(std::max(1.0, initial_height)) {
  registry_.Register("text-reflow", &CreateTextReflow);
  registry_.Register("text-cutting", &CreateTextCutting);
  registry_.Register("text-tracing", &CreateTextTracing);
  registry_.Register("text-morphing", &CreateTextMorphing);
  registry_.Register("curve-interpolate", &CreateCurveInterpolate);
  registry_.Register("env-distort", &CreateEnvelopeDistort);
  registry_.Register("mesh-warp", &CreateMeshWarp);
  registry_.Register("swirl-deform", &CreateSwirlDeform);
  registry_.Register("noise-deform", &CreateNoiseDeform);
  registry_.Register("pucker-bloat", &CreatePuckerBloat);
  registry_.Register("shape-intersection", &CreateShapeIntersection);
  registry_.Register("contour-lines", &CreateContourLines);
  registry_.Register("contour-composite", &CreateContourComposite);
  registry_.Register("sksl-shader", &CreateSkslShader);
  registry_.Register("scene-benchmark-webgl", &CreateSceneBenchmarkWebGl);
  registry_.Register("scene-benchmark-cpu", &CreateSceneBenchmarkCpu);

  std::cout << "[cc-engine/stdout] FiddleManager initialized with "
            << "text-reflow, text-cutting, text-tracing, text-morphing, "
               "curve-interpolate, env-distort, "
               "mesh-warp, swirl-deform, noise-deform, pucker-bloat, "
               "shape-intersection, "
               "contour-lines, contour-composite, sksl-shader, "
               "scene-benchmark-webgl, and "
               "scene-benchmark-cpu."
            << std::endl;
  std::cerr << "[cc-engine/stderr] C++ canvas error stream is connected."
            << std::endl;

  if (!SelectFiddle(initial_key)) {
    SelectFiddle("text-reflow");
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

bool FiddleManager::IsSvgWritable() const {
  return active_fiddle_ != nullptr && active_fiddle_->IsSvgWritable();
}

std::string FiddleManager::ExportSvg() {
  if (!IsSvgWritable()) {
    return {};
  }
  return active_fiddle_->ExportSvg();
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
