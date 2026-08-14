#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "include/core/SkRefCnt.h"

class SkImage;

class SkiaImageStore final {
public:
  static SkiaImageStore &Instance();

  bool RegisterRgbaImage(const std::string &image_id,
                         const std::uint8_t *pixels, int width, int height);
  sk_sp<SkImage> ImageForId(const std::string &image_id) const;

  SkiaImageStore(const SkiaImageStore &) = delete;
  SkiaImageStore &operator=(const SkiaImageStore &) = delete;

private:
  SkiaImageStore() = default;
  ~SkiaImageStore() = default;

  std::unordered_map<std::string, sk_sp<SkImage>> images_;
};
