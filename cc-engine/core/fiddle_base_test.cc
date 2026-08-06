#include "core/fiddle_base.h"

#include <cassert>
#include <memory>

namespace {

class FakeWebGlCanvasResource final : public WebGlCanvasResource {
public:
  bool Resize(int pixel_width, int pixel_height) override {
    width_ = pixel_width;
    height_ = pixel_height;
    return true;
  }

  int PixelWidth() const override { return width_; }
  int PixelHeight() const override { return height_; }
  bool MakeCurrent() override { return true; }
  int Version() const override { return 2; }

private:
  int width_ = 300;
  int height_ = 150;
};

class FakeCpuCanvasResource final : public CpuCanvasResource {
public:
  bool Resize(int pixel_width, int pixel_height) override {
    width_ = pixel_width;
    height_ = pixel_height;
    return true;
  }

  int PixelWidth() const override { return width_; }
  int PixelHeight() const override { return height_; }
  void PresentPixels(const std::uint8_t *, int, int, std::size_t) override {}

private:
  int width_ = 300;
  int height_ = 150;
};

class TestWebGlFiddle final : public FiddleBaseWebGL {
public:
  bool IsSvgWritable() const override { return true; }
  void Render(double) override {}

  int TestPixelWidth() const { return PixelWidth(); }
  int TestPixelHeight() const { return PixelHeight(); }
  double TestWidth() const { return Width(); }
  double TestHeight() const { return Height(); }
  int TestWebGlVersion() { return WebGlResource().Version(); }

private:
  void DrawFrame(SkCanvas *, int, int) override {}
};

} // namespace

int main() {
  TestWebGlFiddle fiddle;
  assert(fiddle.Backend() == FiddleBackend::kWebGl);
  assert(!fiddle.PopulateCanvas(std::make_unique<FakeCpuCanvasResource>()));
  assert(fiddle.PopulateCanvas(std::make_unique<FakeWebGlCanvasResource>()));
  assert(fiddle.TestWebGlVersion() == 2);

  fiddle.Resize(320.0, 180.0, 2.0);
  assert(fiddle.TestPixelWidth() == 640);
  assert(fiddle.TestPixelHeight() == 360);
  assert(fiddle.TestWidth() == 320.0);
  assert(fiddle.TestHeight() == 180.0);
  return 0;
}
