// edwide - the correction: the side parts of the picture laid over the side monitors at each Present.
#pragma once

#include "runtime.h"

namespace edwide
  {
  extern std::atomic<bool> hooked;
  extern std::atomic<bool> hook_at_start;
  extern thread_local int depth;

  auto guarded(IDXGISwapChain * chain) noexcept -> void;
  ///\brief the correction turned off for the session by a fault
  [[nodiscard]]
  auto correction_off() noexcept -> bool;
  }  // namespace edwide
