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

  bool RegisterEncodedImage(const std::string &image_id,
                            const std::uint8_t *bytes, std::size_t byte_count);
  sk_sp<SkImage> ImageForId(const std::string &image_id) const;

  SkiaImageStore(const SkiaImageStore &) = delete;
  SkiaImageStore &operator=(const SkiaImageStore &) = delete;

private:
  SkiaImageStore();
  ~SkiaImageStore() = default;

  std::unordered_map<std::string, sk_sp<SkImage>> images_;
};
