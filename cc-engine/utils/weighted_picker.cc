#include "utils/weighted_picker.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace utils {

WeightedPicker::WeightedPicker(std::vector<double> weights) {
  cumulative_weights_.reserve(weights.size());
  for (double weight : weights) {
    if (std::isfinite(weight) && weight > 0.0)
      total_weight_ += weight;
    cumulative_weights_.push_back(total_weight_);
  }
  if (total_weight_ <= 0.0)
    cumulative_weights_.clear();
}

std::size_t WeightedPicker::PickOne(std::mt19937 &random) const {
  if (empty())
    return kInvalidIndex;
  const double sample =
      std::generate_canonical<double, 53>(random) * total_weight_;
  return static_cast<std::size_t>(std::upper_bound(cumulative_weights_.begin(),
                                                   cumulative_weights_.end(),
                                                   sample) -
                                  cumulative_weights_.begin());
}

std::vector<std::size_t>
WeightedPicker::PickSamples(std::size_t count, std::mt19937 &random) const {
  std::vector<std::size_t> samples;
  if (empty())
    return samples;
  samples.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    samples.push_back(PickOne(random));
  }
  return samples;
}

} // namespace utils
