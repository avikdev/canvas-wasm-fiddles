#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class SkCanvas;

enum class FiddleBackend {
  kWebGl,
  kCpu,
};

// Platform-neutral canvas resources. Platform adapters fill these interfaces
// with the native handles needed by the Skia backends.
class FiddleCanvasResource {
public:
  virtual ~FiddleCanvasResource() = default;

  virtual FiddleBackend Backend() const = 0;
  virtual bool Resize(int pixel_width, int pixel_height) = 0;
  virtual int PixelWidth() const = 0;
  virtual int PixelHeight() const = 0;
};

class WebGlCanvasResource : public FiddleCanvasResource {
public:
  FiddleBackend Backend() const final { return FiddleBackend::kWebGl; }

  virtual bool MakeCurrent() = 0;
  virtual int Version() const = 0;
};

class CpuCanvasResource : public FiddleCanvasResource {
public:
  FiddleBackend Backend() const final { return FiddleBackend::kCpu; }

  virtual void PresentPixels(const std::uint8_t *pixels, int width, int height,
                             std::size_t byte_length) = 0;
};

class FiddleCanvasResourceProvider {
public:
  virtual ~FiddleCanvasResourceProvider() = default;

  virtual std::unique_ptr<FiddleCanvasResource>
  Create(FiddleBackend backend) = 0;
};

class FiddleBase {
public:
  virtual ~FiddleBase();

  FiddleBackend Backend() const;
  bool PopulateCanvas(std::unique_ptr<FiddleCanvasResource> canvas);
  void Resize(double width, double height, double device_pixel_ratio);
  std::string ExportSvg();

  virtual bool IsSvgWritable() const = 0;
  virtual void Render(double time_seconds) = 0;

protected:
  explicit FiddleBase(FiddleBackend backend);

  int PixelWidth() const;
  int PixelHeight() const;
  double Width() const;
  double Height() const;
  FiddleCanvasResource &CanvasResource();

  virtual void DrawFrame(SkCanvas *canvas, int width, int height) = 0;

private:
  void RefreshDimensions();

  FiddleBackend backend_;
  std::unique_ptr<FiddleCanvasResource> canvas_;
  double device_pixel_ratio_ = 1.0;
  double width_ = 1.0;
  double height_ = 1.0;
};

class FiddleBaseWebGL : public FiddleBase {
protected:
  FiddleBaseWebGL();

  WebGlCanvasResource &WebGlResource();
};

class FiddleBaseCpu : public FiddleBase {
protected:
  FiddleBaseCpu();

  CpuCanvasResource &CpuResource();
};
