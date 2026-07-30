#pragma once

#include <emscripten/val.h>

class FiddleBase {
public:
  virtual ~FiddleBase() = default;

  void PopulateCanvas(const emscripten::val &canvas);
  void Resize(double width, double height, double device_pixel_ratio);

  virtual void Render(double time_seconds) = 0;

protected:
  virtual bool UsesWebGl() const;

  emscripten::val &Canvas();
  emscripten::val &Context();
  double Width() const;
  double Height() const;

private:
  void RefreshDimensions();

  emscripten::val canvas_ = emscripten::val::undefined();
  emscripten::val context_ = emscripten::val::undefined();
  double device_pixel_ratio_ = 1.0;
  double width_ = 1.0;
  double height_ = 1.0;
};
