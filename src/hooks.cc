// edwide - the hook: Present in the swap chain's table, put in place through the device's DXGI factory.
#include "runtime.h"
#include "hooks.h"
#include "correction.h"

#include <edplugin_common/core/once_flag.h>

#include <initializer_list>

namespace edwide
  {
  // ---- the hook: Present and Present1 in the swap chain's table of methods ----
  using present_fn = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT);
  using present1_fn = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain1 *, UINT, UINT, DXGI_PRESENT_PARAMETERS const *);
  present_fn orig_present{};
  present1_fn orig_present1{};

  HRESULT STDMETHODCALLTYPE hook_present(IDXGISwapChain * chain, UINT sync, UINT flags)
    {
    if(not(flags & DXGI_PRESENT_TEST))
      guarded(chain);
    ++depth;  // a Present1 that calls Present inside is corrected once
    HRESULT const hr{orig_present(chain, sync, flags)};
    --depth;
    return hr;
    }

  HRESULT STDMETHODCALLTYPE hook_present1(IDXGISwapChain1 * chain, UINT sync, UINT flags, DXGI_PRESENT_PARAMETERS const * params)
    {
    if(not(flags & DXGI_PRESENT_TEST))
      guarded(chain);
    ++depth;
    HRESULT const hr{orig_present1(chain, sync, flags, params)};
    --depth;
    return hr;
    }

  auto patch(void ** table, int slot, void * hook, void ** original) -> bool
    {
    DWORD old{};
    if(not VirtualProtect(&table[slot], sizeof(void *), PAGE_READWRITE, &old))
      return false;
    *original = table[slot];
    table[slot] = hook;
    VirtualProtect(&table[slot], sizeof(void *), old, &old);
    FlushInstructionCache(GetCurrentProcess(), &table[slot], sizeof(void *));
    return true;
    }

  // ---- the hook put in place: through the DXGI factory, no device of its own (2026-11-18: a probe device and swap chain
  // of the system's d3d11 made beside the game's ended the game at its start, with EDHM in edloader's list) ----
  using create_chain_fn = HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **);
  using create_hwnd_fn = HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory2 *, IUnknown *, HWND, DXGI_SWAP_CHAIN_DESC1 const *,
                                                      DXGI_SWAP_CHAIN_FULLSCREEN_DESC const *, IDXGIOutput *, IDXGISwapChain1 **);
  create_chain_fn orig_create_chain{};
  create_hwnd_fn orig_create_hwnd{};
  SRWLOCK chain_lock = SRWLOCK_INIT;
  constinit edplugin_common::guard_t hook_guard{"hook", 1};
  constinit edplugin_common::once_flag_t factory_once{};

  ///\brief a swap chain the game made: Present and Present1 hooked in its table, once a table
  auto hook_chain(IDXGISwapChain * chain) noexcept -> void
    {
    if(not chain)
      return;
    exclusive_lock_t const hold{chain_lock};
    void ** const table{*reinterpret_cast<void ***>(chain)};
    if(table[8] != reinterpret_cast<void *>(&hook_present))
      {
      bool ok{patch(table, 8, reinterpret_cast<void *>(&hook_present), reinterpret_cast<void **>(&orig_present))};
      IDXGISwapChain1 * chain1{};
      if(SUCCEEDED(chain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void **>(&chain1))) and chain1)
        {
        void ** const table1{*reinterpret_cast<void ***>(chain1)};
        if(table1[22] != reinterpret_cast<void *>(&hook_present1))
          ok = ok and patch(table1, 22, reinterpret_cast<void *>(&hook_present1), reinterpret_cast<void **>(&orig_present1));
        chain1->Release();
        }
      log_line("{}", ok ? "hook: Present of the game's swap chain" : "hook: the game's Present could not be hooked; nothing is changed");
      hooked.store(ok);
      }
    }

  HRESULT STDMETHODCALLTYPE hook_create_chain(IDXGIFactory * factory, IUnknown * device, DXGI_SWAP_CHAIN_DESC * desc, IDXGISwapChain ** out)
    {
    HRESULT const hr{orig_create_chain(factory, device, desc, out)};
    if(SUCCEEDED(hr) and out)
      {
      IDXGISwapChain * const chain{*out};
      hook_guarded([chain] { hook_chain(chain); });
      }
    return hr;
    }

  HRESULT STDMETHODCALLTYPE hook_create_hwnd(IDXGIFactory2 * factory, IUnknown * device, HWND window, DXGI_SWAP_CHAIN_DESC1 const * desc,
                                             DXGI_SWAP_CHAIN_FULLSCREEN_DESC const * full, IDXGIOutput * output, IDXGISwapChain1 ** out)
    {
    HRESULT const hr{orig_create_hwnd(factory, device, window, desc, full, output, out)};
    if(SUCCEEDED(hr) and out)
      {
      IDXGISwapChain * const chain{*out};
      hook_guarded([chain] { hook_chain(chain); });
      }
    return hr;
    }

  ///\brief the factory the game's device belongs to (its adapter's parent): CreateSwapChain and CreateSwapChainForHwnd
  /// hooked in its table, once. The game's own factory, made when it likes, is of the same class (Windows' dxgi,
  /// DXVK's), so its swap chain comes through them; Present is hooked in that swap chain's table
  auto hook_factory_of(ID3D11Device * device) noexcept -> void
    {
    if(not device or not factory_once.first())
      return;
    if(test_fault_at(test_fault_e::hook))
      RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);  // the test's: a fault before anything was hooked
    IDXGIDevice * dxgi_device{};
    IDXGIAdapter * adapter{};
    IDXGIFactory * factory{};
    if(SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi_device))) and dxgi_device
       and SUCCEEDED(dxgi_device->GetAdapter(&adapter)) and adapter
       and SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory))) and factory)
      {
      void ** const table{*reinterpret_cast<void ***>(factory)};
      bool ok{patch(table, 10, reinterpret_cast<void *>(&hook_create_chain), reinterpret_cast<void **>(&orig_create_chain))};
      IDXGIFactory2 * factory2{};
      if(SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&factory2))) and factory2)
        {
        void ** const table2{*reinterpret_cast<void ***>(factory2)};
        ok = ok and patch(table2, 15, reinterpret_cast<void *>(&hook_create_hwnd), reinterpret_cast<void **>(&orig_create_hwnd));
        factory2->Release();
        }
      log_line("{}", ok ? "hook: the device's DXGI factory (CreateSwapChain, CreateSwapChainForHwnd), at the game's D3D11CreateDevice"
                  : "hook: the DXGI factory could not be hooked; nothing is changed");
      }
    else
      log_line("hook: no DXGI factory from the device; nothing is changed");
    for(IUnknown * u: std::initializer_list<IUnknown *>{factory, adapter, dxgi_device})
      if(u)
        u->Release();
    }
  }  // namespace edwide
