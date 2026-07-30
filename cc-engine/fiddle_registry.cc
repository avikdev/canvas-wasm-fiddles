#include "fiddle_registry.h"

bool FiddleRegistry::Register(const std::string& key, Creator creator) {
  if (key.empty() || creator == nullptr) {
    return false;
  }
  return creators_.emplace(key, creator).second;
}

std::unique_ptr<FiddleBase> FiddleRegistry::Create(
    const std::string& key) const {
  const auto entry = creators_.find(key);
  if (entry == creators_.end()) {
    return nullptr;
  }
  return entry->second();
}

bool FiddleRegistry::Contains(const std::string& key) const {
  return creators_.contains(key);
}
