// edwide_screen: a small element of edloader's chain: its D3D11CreateDevice (and ...AndSwapChain) passes the call on
// and hooks the device's DXGI factory before the game can make its swap chain (no thread, no device of its own; a
// thread of its own lost that race, and disturbed EDHM, 2026-11-18). Its hook, IDXGISwapChain::Present (and Present1), lays the side parts of the
// finished picture over side monitors set at an angle (shaders/wings.hlsl, src/wings_math.h), after the game's
// upscaling and HUD, before the picture is shown. The picture is taken at each Present and let go before it, so the
// game's ResizeBuffers never meets a reference of ours; a picture whose size is not the layout's passes untouched,
// said once in the log, and the correction comes back when it is.
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <shlobj.h>

#include "wings_math.h"

#include "wings_ps.h"
#include "wings_vs.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

namespace
  {
  using namespace edwide;

  HMODULE self{};
  std::wstring config_dir, log_dir;
  HANDLE log_file{INVALID_HANDLE_VALUE};
  SRWLOCK log_lock = SRWLOCK_INIT;

  auto log_line(char const * fmt, ...) noexcept -> void
    {
    if(log_file == INVALID_HANDLE_VALUE)
      return;
    SYSTEMTIME t;
    GetSystemTime(&t);
    char buf[1024];
    int n{std::snprintf(buf, sizeof buf, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                        t.wSecond, t.wMilliseconds)};
    va_list args;
    va_start(args, fmt);
    int const room{static_cast<int>(sizeof buf) - n - 1};
    int const m{std::vsnprintf(buf + n, static_cast<std::size_t>(room), fmt, args)};
    va_end(args);
    n += m < 0 ? 0 : std::min(m, room - 1);
    buf[n++] = '\n';
    DWORD written{};
    AcquireSRWLockExclusive(&log_lock);
    WriteFile(log_file, buf, static_cast<DWORD>(n), &written, nullptr);
    ReleaseSRWLockExclusive(&log_lock);
    }

  ///\brief a folder edloader hands its plugins (EDLOADER_CONFIG_DIR, EDLOADER_LOG_DIR); without edloader, the dll's own
  auto directory_from(wchar_t const * variable) -> std::wstring
    {
    wchar_t value[MAX_PATH]{};
    DWORD const n{GetEnvironmentVariableW(variable, value, MAX_PATH)};
    if(n != 0 and n < MAX_PATH)
      return std::wstring{value, n};
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring dir{path};
    if(auto const slash{dir.find_last_of(L"\\/")}; slash != std::wstring::npos)
      dir.resize(slash);
    return dir;
    }

  auto write_time(std::wstring const & path) noexcept -> ULONGLONG
    {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if(not GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a))
      return 0;
    return (static_cast<ULONGLONG>(a.ftLastWriteTime.dwHighDateTime) << 32) | a.ftLastWriteTime.dwLowDateTime;
    }

  auto read_text(std::wstring const & path) -> std::string
    {
    std::string text;
    HANDLE const f{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(f == INVALID_HANDLE_VALUE)
      return text;
    char buf[4096];
    DWORD got{};
    while(ReadFile(f, buf, sizeof buf, &got, nullptr) and got != 0)
      text.append(buf, got);
    CloseHandle(f);
    return text;
    }

  // ---- the settings (edwide_screen.ini, read again within a second of a change) ----
  struct settings_t
    {
    float wings{0.f};          ///< 1 = on
    float angle{30.f};         ///< degrees, the sides turned towards the eye
    float centre_px{0.f};      ///< the middle monitor's width in pixels of the picture; 0 = not set (off)
    float side_px{0.f};        ///< each side monitor's width; 0 = the rest of the picture
    float bezel_px{0.f};       ///< the picture's pixels behind the bezels between the middle and a side
    float fov{0.f};            ///< vertical, degrees; 0 = the game's (Settings.xml)
    float eye_cm{0.f};         ///< the eyes' distance from the middle monitor; with centre_cm the plane's distance itself
    float centre_cm{0.f};      ///< the middle monitor's picture width, cm (pixels a cm: centre_px / centre_cm)
    float zoom{1.f};
    float curve{0.f};
    float offset_y{0.f};
    float calibrate{0.f};
    float hook{1.f};           ///< read at the start: 0 = nothing hooked this session (to find what disturbs another mod)
    };

  struct key_t
    {
    char const * name;
    float settings_t::*field;
    char const * help;
    };

  key_t const keys[]{
    {"wings_hook", &settings_t::hook, "read when the game starts: 1 = the hook in place; 0 = nothing hooked (the calls only passed on), to tell whether another mod is disturbed by it"},
    {"wings", &settings_t::wings, "1 = the sides laid over monitors set at an angle; 0 = off"},
    {"wings_angle", &settings_t::angle, "the side monitors' angle towards you, degrees (0 = in one line with the middle)"},
    {"wings_center_px", &settings_t::centre_px, "the middle monitor's width in pixels of the game's picture; 0 = not set (off)"},
    {"wings_side_px", &settings_t::side_px, "each side monitor's width in pixels; 0 = the rest of the picture (any width)"},
    {"wings_bezel_px", &settings_t::bezel_px, "pixels of the picture behind the bezels between the middle and a side (0 = none)"},
    {"wings_eye_cm", &settings_t::eye_cm, "your eyes' distance from the middle monitor, cm (with wings_center_cm: the sides as seen from there; 0 = from the field of view)"},
    {"wings_center_cm", &settings_t::centre_cm, "the middle monitor's picture width, cm (0 = from the field of view)"},
    {"wings_fov", &settings_t::fov, "only without wings_eye_cm: the game's vertical field of view, degrees; 0 = read from the game's Settings.xml"},
    {"wings_zoom", &settings_t::zoom, "the sides' size: above 1 larger (less black at the corners, more cut off)"},
    {"wings_curve", &settings_t::curve, "0 flat side monitors .. 1 curved ones"},
    {"wings_offset_y", &settings_t::offset_y, "the sides moved up (-) or down (+), pixels"},
    {"wings_calibrate", &settings_t::calibrate, "1 = shapes to set the angle by: circles, squares and rows across the monitors' edges, round, square and straight from where you sit when wings_angle is right"},
  };

  settings_t settings{};
  std::wstring ini_path;
  ULONGLONG ini_written{};
  ULONGLONG last_look_ms{};
  std::wstring game_settings_path;
  ULONGLONG game_settings_written{};
  float game_fov{};

  auto read_ini() -> void
    {
    settings_t s{};
    std::string const text{read_text(ini_path)};
    std::size_t at{};
    while(at < text.size())
      {
      std::size_t const end{std::min(text.find('\n', at), text.size())};
      std::string line{text.substr(at, end - at)};
      at = end + 1;
      if(auto const hash{line.find_first_of(";#")}; hash != std::string::npos)
        line.resize(hash);
      auto const eq{line.find('=')};
      if(eq == std::string::npos)
        continue;
      auto trim = [](std::string v)
        {
        v.erase(0, v.find_first_not_of(" \t\r"));
        v.erase(v.find_last_not_of(" \t\r") + 1);
        return v;
        };
      std::string const key{trim(line.substr(0, eq))}, value{trim(line.substr(eq + 1))};
      bool known{};
      for(key_t const & k: keys)
        if(key == k.name)
          {
          known = true;
          char * stop{};
          float const f{std::strtof(value.c_str(), &stop)};
          if(stop == value.c_str() or *stop != '\0' or not std::isfinite(f))
            log_line("settings: '%s = %s' cannot be read; %s stays as it was", key.c_str(), value.c_str(), key.c_str());
          else
            s.*k.field = f;
          }
      if(not known and not key.empty())
        log_line("settings: no setting '%s' (a typo?): ignored", key.c_str());
      }
    s.zoom = std::clamp(s.zoom, 0.25f, 4.f);
    s.curve = std::clamp(s.curve, 0.f, 1.f);
    s.angle = std::clamp(s.angle, -80.f, 80.f);
    settings = s;
    log_line("settings: wings %g, angle %g, centre %g px, side %g px, bezel %g px, eye %g cm, centre %g cm, fov %g, zoom %g, curve %g, "
             "offset %g, calibrate %g",
             double(s.wings), double(s.angle), double(s.centre_px), double(s.side_px), double(s.bezel_px), double(s.eye_cm),
             double(s.centre_cm), double(s.fov), double(s.zoom), double(s.curve), double(s.offset_y), double(s.calibrate));
    }

  ///\brief the ini with every setting as a comment, when there is none
  auto make_ini() -> void
    {
    if(write_time(ini_path) != 0)
      return;
    std::string text{"; edwide_screen: the sides of the game's picture laid over side monitors set at an angle (README.md)\r\n"
                     "; a line without ';' sets a value; read again within a second of a change\r\n"};
    settings_t const d{};
    for(key_t const & k: keys)
      {
      char line[256];
      std::snprintf(line, sizeof line, "; %s\r\n;%s = %g\r\n", k.help, k.name, double(d.*k.field));
      text += line;
      }
    HANDLE const f{CreateFileW(ini_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(f == INVALID_HANDLE_VALUE)
      return;
    DWORD written{};
    WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(f);
    log_line("settings: %S made, all its lines comments", ini_path.c_str());
    }

  ///\brief the game's vertical field of view from its Settings.xml (`<FOV>`), read again when the file changes
  auto read_game_fov() -> void
    {
    std::string const text{read_text(game_settings_path)};
    auto const at{text.find("<FOV>")};
    float const fov{at == std::string::npos ? 0.f : std::strtof(text.c_str() + at + 5, nullptr)};
    if(fov != game_fov)
      log_line("game: the field of view %g degrees (%S)", double(fov), game_settings_path.c_str());
    game_fov = fov;
    }

  auto look_at_files() -> void
    {
    ULONGLONG const now{GetTickCount64()};
    if(last_look_ms != 0 and now - last_look_ms < 1000u)
      return;
    last_look_ms = now;
    if(ULONGLONG const w{write_time(ini_path)}; w != ini_written)
      {
      ini_written = w;
      read_ini();
      }
    if(ULONGLONG const w{write_time(game_settings_path)}; w != game_settings_written)
      {
      game_settings_written = w;
      read_game_fov();
      }
    }

  // ---- the correction ----
  enum struct state_e
    {
    unknown,
    on,
    off_setting,
    off_layout,
    off_size,
    off_fov,
    off_picture,
    broken
    };

  struct gpu_t
    {
    ID3D11Device * device{};  ///< whose objects below are; held
    ID3D11VertexShader * vs{};
    ID3D11PixelShader * ps{};
    ID3D11SamplerState * sampler{};
    ID3D11Buffer * constants{};
    ID3DDeviceContextState * state{};
    ID3D11Texture2D * copy{};
    ID3D11ShaderResourceView * copy_view{};
    UINT copy_width{}, copy_height{};
    DXGI_FORMAT copy_format{};
    };

  gpu_t gpu{};
  state_e state{state_e::unknown};
  UINT told_width{}, told_height{};
  std::atomic<bool> hooked{};
  bool hook_at_start{true};
  thread_local int depth{};

  auto release(IUnknown * & u) noexcept -> void
    {
    if(u)
      u->Release();
    u = nullptr;
    }

  template<typename T>
  auto release(T * & p) noexcept -> void
    {
    IUnknown * u{p};
    release(u);
    p = nullptr;
    }

  auto release_gpu() noexcept -> void
    {
    release(gpu.copy_view);
    release(gpu.copy);
    release(gpu.state);
    release(gpu.constants);
    release(gpu.sampler);
    release(gpu.ps);
    release(gpu.vs);
    release(gpu.device);
    gpu = gpu_t{};
    }

  auto tell(state_e now, UINT width, UINT height, char const * why) -> void
    {
    if(now == state and width == told_width and height == told_height)
      return;
    bool const again{state != state_e::unknown};
    state = now;
    told_width = width;
    told_height = height;
    if(now == state_e::on)
      log_line("%s: the picture %ux%u, the sides laid over the monitors at %g degrees, the eye %s", again ? "on again" : "on", width,
               height, double(settings.angle), settings.eye_cm > 0.f and settings.centre_cm > 0.f ? "as set (wings_eye_cm)" : "by the field of view");
    else
      log_line("off: %s", why);
    }

  ///\brief the device's objects, made once for it (and the copy for the picture's size and format)
  auto make_gpu(ID3D11Device * device, D3D11_TEXTURE2D_DESC const & d) -> bool
    {
    if(gpu.device != device)
      {
      release_gpu();
      ID3D11Device1 * device1{};
      if(FAILED(device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void **>(&device1))) or not device1)
        return false;
      D3D_FEATURE_LEVEL const level{device->GetFeatureLevel()};
      HRESULT const hr{device1->CreateDeviceContextState(0, &level, 1, D3D11_SDK_VERSION, __uuidof(ID3D11Device1), nullptr, &gpu.state)};
      device1->Release();
      if(FAILED(hr))
        return false;
      device->AddRef();
      gpu.device = device;
      D3D11_SAMPLER_DESC s{};
      s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      s.MaxLOD = D3D11_FLOAT32_MAX;
      D3D11_BUFFER_DESC b{};
      b.ByteWidth = 48;
      b.Usage = D3D11_USAGE_DEFAULT;
      b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      if(FAILED(device->CreateVertexShader(g_wings_vs, sizeof g_wings_vs, nullptr, &gpu.vs))
         or FAILED(device->CreatePixelShader(g_wings_ps, sizeof g_wings_ps, nullptr, &gpu.ps))
         or FAILED(device->CreateSamplerState(&s, &gpu.sampler)) or FAILED(device->CreateBuffer(&b, nullptr, &gpu.constants)))
        return false;
      }
    if(gpu.copy and gpu.copy_width == d.Width and gpu.copy_height == d.Height and gpu.copy_format == d.Format)
      return true;
    release(gpu.copy_view);
    release(gpu.copy);
    D3D11_TEXTURE2D_DESC c{d};
    c.MipLevels = c.ArraySize = 1;
    c.Usage = D3D11_USAGE_DEFAULT;
    c.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    c.CPUAccessFlags = 0;
    c.MiscFlags = 0;
    if(FAILED(device->CreateTexture2D(&c, nullptr, &gpu.copy)) or FAILED(device->CreateShaderResourceView(gpu.copy, nullptr, &gpu.copy_view)))
      {
      release(gpu.copy_view);
      release(gpu.copy);
      return false;
      }
    gpu.copy_width = d.Width;
    gpu.copy_height = d.Height;
    gpu.copy_format = d.Format;
    return true;
    }

  auto correct(IDXGISwapChain * chain) -> void
    {
    look_at_files();
    if(settings.wings < 0.5f)
      return tell(state_e::off_setting, 0, 0, "wings = 0 in the ini");
    ID3D11Device * device{};
    if(FAILED(chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void **>(&device))) or not device)
      return;
    ID3D11Texture2D * picture{};
    chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&picture));
    struct held_t
      {
      ID3D11Device * & d;
      ID3D11Texture2D * & p;
      ~held_t()
        {
        if(p)
          p->Release();
        if(d)
          d->Release();
        }
      } const held{device, picture};
    if(not picture)
      return;
    D3D11_TEXTURE2D_DESC d{};
    picture->GetDesc(&d);
    char why[192];
    if(d.SampleDesc.Count != 1)
      return tell(state_e::off_picture, d.Width, d.Height, "the picture is multisampled");
    if(settings.centre_px <= 0.f)
      return tell(state_e::off_layout, d.Width, d.Height, "wings_center_px is not set");
    float const side{settings.side_px > 0.f ? settings.side_px : 0.5f * (static_cast<float>(d.Width) - settings.centre_px) - settings.bezel_px};
    float const expected{settings.centre_px + 2.f * (side + settings.bezel_px)};
    if(settings.side_px > 0.f and std::fabs(expected - static_cast<float>(d.Width)) > 0.5f)
      {
      std::snprintf(why, sizeof why, "the picture is %ux%u, the settings expect a width of %.0f", d.Width, d.Height, double(expected));
      return tell(state_e::off_size, d.Width, d.Height, why);
      }
    if(side <= 0.f)
      {
      std::snprintf(why, sizeof why, "the picture is %ux%u, no room for the sides beside a middle of %.0f", d.Width, d.Height,
                    double(settings.centre_px));
      return tell(state_e::off_size, d.Width, d.Height, why);
      }
    // the picture plane's distance from the eye: the eyes' own distance in pixels when it is set (the sides then go on
    // from the middle monitor as seen from there), else the one the game's field of view draws for
    float plane{};
    if(settings.eye_cm > 0.f and settings.centre_cm > 0.f)
      plane = settings.eye_cm * settings.centre_px / settings.centre_cm;
    else
      {
      float const fov{settings.fov > 0.f ? settings.fov : game_fov};
      if(not(fov > 1.f and fov < 179.f))
        return tell(state_e::off_fov, d.Width, d.Height, "no distance: wings_eye_cm and wings_center_cm not set, and no field of view");
      plane = wings_plane(fov, static_cast<float>(d.Height));
      }
    if(not make_gpu(device, d))
      return tell(state_e::off_picture, d.Width, d.Height, "the device's objects or the picture's copy could not be made");
    ID3D11RenderTargetView * target{};
    if(FAILED(device->CreateRenderTargetView(picture, nullptr, &target)) or not target)
      return tell(state_e::off_picture, d.Width, d.Height, "the picture cannot be drawn into");
    tell(state_e::on, d.Width, d.Height, "");

    float const a{settings.angle * 0.017453293f};
    float const constants[12]{static_cast<float>(d.Width), static_cast<float>(d.Height), 0.5f * settings.centre_px, settings.bezel_px,
                              plane, std::cos(a), std::sin(a), settings.curve,
                              settings.zoom, settings.offset_y, settings.calibrate, 0.f};
    ID3D11DeviceContext * ctx{};
    device->GetImmediateContext(&ctx);
    ID3D11DeviceContext1 * ctx1{};
    if(ctx)
      ctx->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void **>(&ctx1));
    if(ctx1)
      {
      // the game's whole state set aside and put back as it was (one call each way)
      ID3DDeviceContextState * game{};
      ctx1->SwapDeviceContextState(gpu.state, &game);
      ctx1->CopyResource(gpu.copy, picture);
      ctx1->UpdateSubresource(gpu.constants, 0, nullptr, constants, 0, 0);
      D3D11_VIEWPORT const port{0.f, 0.f, static_cast<float>(d.Width), static_cast<float>(d.Height), 0.f, 1.f};
      ctx1->OMSetRenderTargets(1, &target, nullptr);
      ctx1->RSSetViewports(1, &port);
      ctx1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx1->VSSetShader(gpu.vs, nullptr, 0);
      ctx1->PSSetShader(gpu.ps, nullptr, 0);
      ctx1->PSSetShaderResources(0, 1, &gpu.copy_view);
      ctx1->PSSetSamplers(0, 1, &gpu.sampler);
      ctx1->PSSetConstantBuffers(0, 1, &gpu.constants);
      ctx1->Draw(3, 0);
      ID3D11ShaderResourceView * const none{};
      ctx1->PSSetShaderResources(0, 1, &none);
      ID3D11RenderTargetView * const no_target{};
      ctx1->OMSetRenderTargets(1, &no_target, nullptr);
      ctx1->SwapDeviceContextState(game, nullptr);
      if(game)
        game->Release();
      ctx1->Release();
      }
    if(ctx)
      ctx->Release();
    target->Release();
    }

  auto correct_caught(IDXGISwapChain * chain) -> bool
    {
    try
      {
      correct(chain);
      return true;
      }
    catch(...)
      {
      return false;
      }
    }

  auto guarded(IDXGISwapChain * chain) noexcept -> void
    {
    if(state == state_e::broken or depth != 0)
      return;
    ++depth;
    bool ok{};
    DWORD code{};
    __try
      {
      ok = correct_caught(chain);
      }
    __except(code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
      {
      ok = false;
      }
    --depth;
    if(not ok)
      {
      state = state_e::broken;
      log_line("off for this session: a fault in the correction (exception 0x%08lX); the game goes on without it", code);
      }
    }

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

  ///\brief a swap chain the game made: Present and Present1 hooked in its table, once a table
  auto hook_chain(IDXGISwapChain * chain) noexcept -> void
    {
    if(not chain)
      return;
    AcquireSRWLockExclusive(&chain_lock);
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
      log_line(ok ? "hook: Present of the game's swap chain" : "hook: the game's Present could not be hooked; nothing is changed");
      hooked.store(ok);
      }
    ReleaseSRWLockExclusive(&chain_lock);
    }

  HRESULT STDMETHODCALLTYPE hook_create_chain(IDXGIFactory * factory, IUnknown * device, DXGI_SWAP_CHAIN_DESC * desc, IDXGISwapChain ** out)
    {
    HRESULT const hr{orig_create_chain(factory, device, desc, out)};
    if(SUCCEEDED(hr) and out)
      hook_chain(*out);
    return hr;
    }

  HRESULT STDMETHODCALLTYPE hook_create_hwnd(IDXGIFactory2 * factory, IUnknown * device, HWND window, DXGI_SWAP_CHAIN_DESC1 const * desc,
                                             DXGI_SWAP_CHAIN_FULLSCREEN_DESC const * full, IDXGIOutput * output, IDXGISwapChain1 ** out)
    {
    HRESULT const hr{orig_create_hwnd(factory, device, window, desc, full, output, out)};
    if(SUCCEEDED(hr) and out)
      hook_chain(*out);
    return hr;
    }

  ///\brief the factory the game's device belongs to (its adapter's parent): CreateSwapChain and CreateSwapChainForHwnd
  /// hooked in its table, once. The game's own factory, made when it likes, is of the same class (Windows' dxgi,
  /// DXVK's), so its swap chain comes through them; Present is hooked in that swap chain's table
  auto hook_factory_of(ID3D11Device * device) noexcept -> void
    {
    static bool done{};
    if(done or not device)
      return;
    done = true;
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
      log_line(ok ? "hook: the device's DXGI factory (CreateSwapChain, CreateSwapChainForHwnd), at the game's D3D11CreateDevice"
                  : "hook: the DXGI factory could not be hooked; nothing is changed");
      }
    else
      log_line("hook: no DXGI factory from the device; nothing is changed");
    for(IUnknown * u: std::initializer_list<IUnknown *>{factory, adapter, dxgi_device})
      if(u)
        u->Release();
    }

  auto start() -> void
    {
    config_dir = directory_from(L"EDLOADER_CONFIG_DIR");
    log_dir = directory_from(L"EDLOADER_LOG_DIR");
    log_file = CreateFileW((log_dir + L"\\edwide_screen.log").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    log_line("edwide_screen %s", EDWIDE_VERSION);
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
  }  // namespace

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
