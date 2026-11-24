// edwide_screen: a small element of edloader's chain: its D3D11CreateDevice (and ...AndSwapChain) passes the call on
// and hooks the device's DXGI factory before the game can make its swap chain (no thread, no device of its own; a
// thread of its own lost that race, and disturbed EDHM, 2026-11-18). Its hook, IDXGISwapChain::Present (and Present1), lays the side parts of the
// finished picture over side monitors set at an angle (shaders/wings.hlsl, src/wings_math.h), after the game's
// upscaling and HUD, before the picture is shown. The picture is taken at each Present and let go before it, so the
// game's ResizeBuffers never meets a reference of ours; a picture whose size is not the layout's passes untouched,
// said once in the log, and the correction comes back when it is.
#include "runtime.h"
#include "correction.h"
#include "hooks.h"
#include "edplugin_common_commit.h"

#include <shlobj.h>

#include <string>

namespace edwide
  {
  auto start() -> void
    {
    config_dir = directory_from(L"EDLOADER_CONFIG_DIR");
    log_dir = directory_from(L"EDLOADER_LOG_DIR");
    log_open(log_dir, L"edwide_screen");  // edplugin_common's: the last session's log set aside in edwide_screen\ beside it
    log_line("edwide_screen {}", EDWIDE_VERSION);
    log_line("edplugin_common {}", EDPLUGIN_COMMON_COMMIT);
    ini_path = config_dir + L"\\edwide_screen.ini";
    make_ini();
    PWSTR local{};
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)) and local)
      {
      game_settings_path = std::wstring{local} + L"\\Frontier Developments\\Elite Dangerous\\Options\\Graphics\\Settings.xml";
      CoTaskMemFree(local);
      }
    look_at_files();
    hook_at_start = settings.hook >= 0.5f;
    if(not hook_at_start)
      log_line("hook: wings_hook = 0: nothing is hooked this session (the calls only passed on)");
    }

  INIT_ONCE start_once = INIT_ONCE_STATIC_INIT;

  ///\brief the start, once, at the game's first device (not under the loader lock), never letting anything out
  auto start_once_caught() noexcept -> void
    {
    InitOnceExecuteOnce(
      &start_once,
      [](PINIT_ONCE, PVOID, PVOID *) -> BOOL
        {
        __try
          {
          [] { try { start(); } catch(...) { log_line("start: an exception; nothing is hooked"); hook_at_start = false; } }();
          }
        __except(EXCEPTION_EXECUTE_HANDLER)
          {
          hook_at_start = false;
          }
        return TRUE;
        },
      nullptr, nullptr);
    }

  ///\brief the next d3d11 of edloader's chain: d3d11.dll by name is edloader, which takes it on to the element after
  /// this one (or the system's d3d11 without edloader)
  template<typename F>
  auto next(char const * name) noexcept -> F
    {
    HMODULE const d3d11{LoadLibraryW(L"d3d11.dll")};
    return d3d11 ? reinterpret_cast<F>(GetProcAddress(d3d11, name)) : nullptr;
    }

  auto hook_device_caught(ID3D11Device * device, IDXGISwapChain * chain) noexcept -> void
    {
    if(not hook_at_start)
      return;
    __try
      {
      hook_factory_of(device);
      if(chain)
        hook_chain(chain);
      }
    __except(EXCEPTION_EXECUTE_HANDLER)
      {
      log_line("hook: a fault while hooking; nothing more is hooked");
      hook_at_start = false;
      }
    }
  }  // namespace edwide

using namespace edwide;


// the chain element's two calls (exported as D3D11CreateDevice and D3D11CreateDeviceAndSwapChain, CMakeLists.txt):
// passed on first, the factory hooked on the device they made, before the game can make its swap chain
extern "C" HRESULT WINAPI edwide_create_device(IDXGIAdapter * adapter, D3D_DRIVER_TYPE type, HMODULE software, UINT flags,
                                               D3D_FEATURE_LEVEL const * levels, UINT count, UINT version, ID3D11Device ** device,
                                               D3D_FEATURE_LEVEL * level, ID3D11DeviceContext ** ctx)
  {
  start_once_caught();
  auto const call{next<PFN_D3D11_CREATE_DEVICE>("D3D11CreateDevice")};
  if(not call)
    return E_FAIL;
  HRESULT const hr{call(adapter, type, software, flags, levels, count, version, device, level, ctx)};
  if(SUCCEEDED(hr) and device and *device)
    hook_device_caught(*device, nullptr);
  return hr;
  }

extern "C" HRESULT WINAPI edwide_create_device_and_chain(IDXGIAdapter * adapter, D3D_DRIVER_TYPE type, HMODULE software, UINT flags,
                                                         D3D_FEATURE_LEVEL const * levels, UINT count, UINT version,
                                                         DXGI_SWAP_CHAIN_DESC const * desc, IDXGISwapChain ** chain, ID3D11Device ** device,
                                                         D3D_FEATURE_LEVEL * level, ID3D11DeviceContext ** ctx)
  {
  start_once_caught();
  auto const call{next<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>("D3D11CreateDeviceAndSwapChain")};
  if(not call)
    return E_FAIL;
  HRESULT const hr{call(adapter, type, software, flags, levels, count, version, desc, chain, device, level, ctx)};
  if(SUCCEEDED(hr) and device and *device)
    hook_device_caught(*device, chain ? *chain : nullptr);
  return hr;
  }

///\brief for edworld's build_info (a line over its own, 2026-11-19): "edwide_screen <version> wings <angle>" (or "wings
/// off", "hook 0"), NUL-ended within size; its length (0: no room)
extern "C" __declspec(dllexport) auto edwide_screen_build_info(char * text, int size) -> int
  {
  if(not text or size <= 0)
    return 0;
  if(not hook_at_start)
    return static_cast<int>(format_into(text, static_cast<std::size_t>(size), "edwide_screen {} hook 0", EDWIDE_VERSION));
  if(settings.wings < 0.5f)
    return static_cast<int>(format_into(text, static_cast<std::size_t>(size), "edwide_screen {} wings off", EDWIDE_VERSION));
  return static_cast<int>(format_into(text, static_cast<std::size_t>(size), "edwide_screen {} wings {:g} deg", EDWIDE_VERSION, double(settings.angle)));
  }

///\brief for the test: 1 once a swap chain's Present is hooked
extern "C" __declspec(dllexport) auto edwide_screen_hooked() -> int
  {
  return hooked.load() ? 1 : 0;
  }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
  {
  if(reason == DLL_PROCESS_ATTACH)
    {
    self = instance;
    DisableThreadLibraryCalls(instance);
    // nothing under the loader lock: the start comes with the game's first D3D11CreateDevice through this element
    }
  return TRUE;
  }
