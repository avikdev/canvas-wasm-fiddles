#include "images/skia_image_store.h"

#include <iostream>
#include <utility>

#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"

SkiaImageStore &SkiaImageStore::Instance() {
  static SkiaImageStore instance;
  return instance;
}

bool SkiaImageStore::RegisterRgbaImage(const std::string &image_id,
                                       const std::uint8_t *pixels, int width,
                                       int height) {
  if (image_id.empty() || pixels == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  const SkImageInfo info = SkImageInfo::Make(
      width, height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  const SkPixmap pixmap(info, pixels, info.minRowBytes());
  sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(pixmap);
  if (image == nullptr) {
    return false;
  }
  images_.insert_or_assign(image_id, std::move(image));
  std::cout << "[cc-engine/stdout] Registered browser-decoded image: id="
            << image_id << ", size=" << width << "x" << height << "."
            << std::endl;
  return true;
}

sk_sp<SkImage> SkiaImageStore::ImageForId(const std::string &image_id) const {
  const auto image = images_.find(image_id);
  return image == images_.end() ? nullptr : image->second;
}
