#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace utils {

class WeightedPicker {
public:
  static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

  explicit WeightedPicker(std::vector<double> weights);

  bool empty() const { return cumulative_weights_.empty(); }
  std::size_t size() const { return cumulative_weights_.size(); }

  std::size_t PickOne(std::mt19937 &random) const;
  std::vector<std::size_t> PickSamples(std::size_t count,
                                       std::mt19937 &random) const;

private:
  std::vector<double> cumulative_weights_;
  double total_weight_ = 0.0;
};

} // namespace utils
