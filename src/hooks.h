// edwide - the hook: Present in the swap chain's table, put in place through the device's DXGI factory.
#pragma once

#include "runtime.h"
#include "correction.h"

#include <edplugin_common/guard/guard.h>

namespace edwide
  {
  ///\brief a swap chain the game made: Present and Present1 hooked in its table, once a table
  auto hook_chain(IDXGISwapChain * chain) noexcept -> void;
  ///\brief the factory the game's device belongs to (its adapter's parent): CreateSwapChain and CreateSwapChainForHwnd
  /// hooked in its table, once. The game's own factory, made when it likes, is of the same class (Windows' dxgi,
  /// DXVK's), so its swap chain comes through them; Present is hooked in that swap chain's table
  auto hook_factory_of(ID3D11Device * device) noexcept -> void;
  ///\brief the guard of the hooks put in place (hook_chain and hook_factory_of, never an original call)
  extern edplugin_common::guard_t hook_guard;

  ///\brief `hook()` under hook_guard: the first fault turns the hooks off for the session (no swap chain gets Present
  /// hooked after it, but one of a class already hooked keeps its correction: the table is the class's),
  /// hook_guard.broken() from then on
  template<typename hook_t>
  auto hook_guarded(hook_t && hook) noexcept -> void
    {
    static_cast<void>(hook_guard.run(hook));
    }
  }  // namespace edwide
