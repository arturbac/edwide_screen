// edwide - the hook: Present in the swap chain's table, put in place through the device's DXGI factory.
#pragma once

#include "runtime.h"

namespace edwide
  {
  ///\brief a swap chain the game made: Present and Present1 hooked in its table, once a table
  auto hook_chain(IDXGISwapChain * chain) noexcept -> void;
  ///\brief the factory the game's device belongs to (its adapter's parent): CreateSwapChain and CreateSwapChainForHwnd
  /// hooked in its table, once. The game's own factory, made when it likes, is of the same class (Windows' dxgi,
  /// DXVK's), so its swap chain comes through them; Present is hooked in that swap chain's table
  auto hook_factory_of(ID3D11Device * device) noexcept -> void;
  }  // namespace edwide
