#include "images/skia_image_store.h"

#include <iostream>
#include <utility>

#include "include/codec/SkCodec.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"

SkiaImageStore &SkiaImageStore::Instance() {
  static SkiaImageStore instance;
  return instance;
}

SkiaImageStore::SkiaImageStore() {
  SkCodecs::Register(SkJpegDecoder::Decoder());
}

bool SkiaImageStore::RegisterEncodedImage(const std::string &image_id,
                                          const std::uint8_t *bytes,
                                          std::size_t byte_count) {
  if (image_id.empty() || bytes == nullptr || byte_count == 0) {
    return false;
  }

  sk_sp<SkData> encoded = SkData::MakeWithCopy(bytes, byte_count);
  sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(std::move(encoded));
  if (image == nullptr) {
    std::cerr << "[cc-engine/stderr] Skia could not decode image: " << image_id
              << std::endl;
    return false;
  }

  const int width = image->width();
  const int height = image->height();
  images_.insert_or_assign(image_id, std::move(image));
  std::cout << "[cc-engine/stdout] Registered image: id=" << image_id
            << ", size=" << width << "x" << height
            << ", encoded-bytes=" << byte_count << "." << std::endl;
  return true;
}

sk_sp<SkImage> SkiaImageStore::ImageForId(const std::string &image_id) const {
  const auto image = images_.find(image_id);
  return image == images_.end() ? nullptr : image->second;
}
