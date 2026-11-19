// edwide - the correction: the side parts of the picture laid over the side monitors at each Present.
#pragma once

#include "runtime.h"

namespace edwide
  {
  extern std::atomic<bool> hooked;
  extern bool hook_at_start;
  extern thread_local int depth;

  auto guarded(IDXGISwapChain * chain) noexcept -> void;
  }  // namespace edwide
