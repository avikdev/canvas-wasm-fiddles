#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "core/fiddle_base.h"

class FiddleRegistry {
public:
  using Creator = std::unique_ptr<FiddleBase> (*)();

  bool Register(const std::string &key, Creator creator);
  std::unique_ptr<FiddleBase> Create(const std::string &key) const;
  bool Contains(const std::string &key) const;

private:
  std::unordered_map<std::string, Creator> creators_;
};
