// edwide - the correction: the side parts of the picture laid over the side monitors at each Present.
#pragma once

#include "runtime.h"

namespace edwide
  {
  extern std::atomic<bool> hooked;
  /// wings_hook as read at the start, the setting only (a fault in the start is entry.cc's start_failed, one while
  /// hooking hook_guard's)
  extern std::atomic<bool> hook_at_start;
  extern thread_local int depth;

  ///\brief for the test only (the export edwide_screen_test_fault): where the next fault comes, once
  enum struct test_fault_e : int
    {
    none,
    start,       ///< in the start, after the log and the ini
    hook,        ///< in the hooking of the device's factory
    correction   ///< in the correction, after the plugin's state took the game's place
    };
  extern std::atomic<test_fault_e> test_fault;

  ///\brief the test's fault armed for `here`: true once (then disarmed)
  [[nodiscard]]
  inline auto test_fault_at(test_fault_e here) noexcept -> bool
    {
    return test_fault.load(std::memory_order_relaxed) == here and test_fault.exchange(test_fault_e::none, std::memory_order_relaxed) == here;
    }

  auto guarded(IDXGISwapChain * chain) noexcept -> void;
  ///\brief the correction turned off for the session by a fault
  [[nodiscard]]
  auto correction_off() noexcept -> bool;
  }  // namespace edwide
