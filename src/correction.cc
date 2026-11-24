// edwide - the correction: the side parts of the picture laid over the side monitors at each Present.
#include "runtime.h"
#include "correction.h"

#include "wings_math.h"
#include "wings_ps.h"
#include "wings_vs.h"

#include <edplugin_common/guard/guard.h>

#include <atomic>
#include <cmath>
#include <cstdio>

namespace edwide
  {
  // ---- the correction ----
  enum struct state_e
    {
    unknown,
    on,
    off_setting,
    off_layout,
    off_size,
    off_fov,
    off_picture
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

  ///\brief what one frame's correction holds while it runs, each set when taken and cleared when let go, so that after
  /// a fault (no destructor run without /EHa) clean_up_after_fault lets go of what is left
  struct frame_t
    {
    ID3D11Device * device;
    ID3D11Texture2D * picture;
    ID3D11RenderTargetView * target;
    ID3D11DeviceContext * ctx;
    ID3D11DeviceContext1 * ctx1;
    ID3DDeviceContextState * game;  ///< the game's state, set aside while the plugin's is in place
    bool swapped;                   ///< the plugin's state in place of the game's
    };

  gpu_t gpu{};
  constinit frame_t frame{};
  state_e state{state_e::unknown};
  UINT told_width{}, told_height{};
  std::atomic<bool> hooked{};
  std::atomic<bool> hook_at_start{true};
  thread_local int depth{};
  // the correction under edplugin_common's guard: the first fault turns it off for the session
  constinit edplugin_common::guard_t correction_guard{"correction", 1};

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
      log_line("{}: the picture {}x{}, the sides laid over the monitors at {:g} degrees, the eye {}", again ? "on again" : "on", width,
               height, double(settings.angle), settings.eye_cm > 0.f and settings.centre_cm > 0.f ? "as set (wings_eye_cm)" : "by the field of view");
    else
      log_line("off: {}", why);
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
    ID3D11Device * & device{frame.device};
    ID3D11Texture2D * & picture{frame.picture};
    struct held_t
      {
      ~held_t()
        {
        release(frame.picture);
        release(frame.device);
        }
      } const held{};
    if(FAILED(chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void **>(&device))) or not device)
      return;
    chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&picture));
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
      format_into(why, "the picture is {}x{}, the settings expect a width of {:.0f}", d.Width, d.Height, double(expected));
      return tell(state_e::off_size, d.Width, d.Height, why);
      }
    if(side <= 0.f)
      {
      format_into(why, "the picture is {}x{}, no room for the sides beside a middle of {:.0f}", d.Width, d.Height,
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
    ID3D11RenderTargetView * & target{frame.target};
    if(FAILED(device->CreateRenderTargetView(picture, nullptr, &target)) or not target)
      {
      release(target);
      return tell(state_e::off_picture, d.Width, d.Height, "the picture cannot be drawn into");
      }
    tell(state_e::on, d.Width, d.Height, "");

    float const a{settings.angle * 0.017453293f};
    float const constants[12]{static_cast<float>(d.Width), static_cast<float>(d.Height), 0.5f * settings.centre_px, settings.bezel_px,
                              plane, std::cos(a), std::sin(a), settings.curve,
                              settings.zoom, settings.offset_y, settings.calibrate, 0.f};
    ID3D11DeviceContext * & ctx{frame.ctx};
    device->GetImmediateContext(&ctx);
    ID3D11DeviceContext1 * & ctx1{frame.ctx1};
    if(ctx)
      ctx->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void **>(&ctx1));
    if(ctx1)
      {
      // the game's whole state set aside and put back as it was (one call each way)
      ID3DDeviceContextState * & game{frame.game};
      ctx1->SwapDeviceContextState(gpu.state, &game);
      frame.swapped = true;
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
      frame.swapped = false;
      release(game);
      release(ctx1);
      }
    release(ctx);
    release(target);
    }

  ///\brief after a fault in correct(), under a guard of its own: the game's state given back when the plugin's was in
  /// its place (the plugin's views unbound first, so its state holds no reference to the picture), what the frame
  /// held let go, and the device's objects released (the correction is off for the session). A fault here leaves the
  /// rest as it was, said in the log by the guard
  auto clean_up_after_fault() -> void
    {
    bool const swapped{frame.swapped and frame.ctx1 and frame.game};
    if(swapped)
      {
      ID3D11ShaderResourceView * const none{};
      frame.ctx1->PSSetShaderResources(0, 1, &none);
      ID3D11RenderTargetView * const no_target{};
      frame.ctx1->OMSetRenderTargets(1, &no_target, nullptr);
      frame.ctx1->SwapDeviceContextState(frame.game, nullptr);
      }
    frame.swapped = false;
    release(frame.game);
    release(frame.ctx1);
    release(frame.ctx);
    release(frame.target);
    release(frame.picture);
    release(frame.device);
    release_gpu();
    log_line("correction: after the fault {}the picture and the device's objects let go", swapped ? "the game's state given back, " : "");
    }

  auto guarded(IDXGISwapChain * chain) noexcept -> void
    {
    // depth outside the guard: a fault there would skip its decrement
    if(depth != 0)
      return;
    ++depth;
    static_cast<void>(correction_guard.run([chain] { correct(chain); }, [](edplugin_common::fault_t const &) { clean_up_after_fault(); }));
    --depth;
    }
  }  // namespace edwide
