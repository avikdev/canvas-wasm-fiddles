#pragma once

#include "fiddle_base.h"

class OrbitalBloomFiddle final : public FiddleBase {
 public:
  void Render(double time_seconds) override;
};
