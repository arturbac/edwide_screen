// edwide_screen's test under wine (tools/test.sh): the plugin loaded as edloader would, a swap chain of a picture
// 960x240 laid out as three monitors (480 in the middle, 240 each side), a smooth picture of known colours (red by x,
// green by y) presented through the hook and read back. Checked: the middle the same bits; each side's pixel the
// colour of where wings_math.h says it comes from; angle 0 changes nothing; wings 0 changes nothing; another size
// passes untouched and is said in the log, the right size again turns it back on; ResizeBuffers is never refused.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "wings_math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <string>
#include <vector>

namespace
  {
  int failures{};

  auto check(bool ok, char const * what) -> void
    {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if(not ok)
      ++failures;
    }

  auto write_ini(char const * text) -> void
    {
    if(FILE * const f{std::fopen("edwide_screen.ini", "wb")})
      {
      std::fputs(text, f);
      std::fclose(f);
      }
    Sleep(1200);  // the plugin looks at the file at most once a second
    }

  auto read_log() -> std::string
    {
    std::string text;
    HANDLE const f{CreateFileW(L"edwide_screen.log", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(f == INVALID_HANDLE_VALUE)
      return text;
    char buf[4096];
    DWORD got{};
    while(ReadFile(f, buf, sizeof buf, &got, nullptr) and got != 0)
      text.append(buf, got);
    CloseHandle(f);
    return text;
    }

  struct app_t
    {
    IDXGISwapChain * chain{};
    ID3D11Device * device{};
    ID3D11DeviceContext * ctx{};
    UINT width{}, height{};
    };

  auto pattern(UINT w, UINT h) -> std::vector<std::uint32_t>
    {
    std::vector<std::uint32_t> p(w * h);
    for(UINT y{}; y != h; ++y)
      for(UINT x{}; x != w; ++x)
        {
        auto const r{static_cast<std::uint32_t>(std::lround((x + 0.5) * 255.0 / w))};
        auto const g{static_cast<std::uint32_t>(std::lround((y + 0.5) * 255.0 / h))};
        p[y * w + x] = r | g << 8 | 128u << 16 | 255u << 24;
        }
    return p;
    }

  ///\brief the pattern put into the picture, presented, and the picture read back
  auto present(app_t const & a, std::vector<std::uint32_t> const * picture = nullptr) -> std::vector<std::uint32_t>
    {
    std::vector<std::uint32_t> const p{picture ? *picture : pattern(a.width, a.height)};
    ID3D11Texture2D * buffer{};
    a.chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&buffer));
    std::vector<std::uint32_t> out(p.size(), 0u);
    if(not buffer)
      return out;
    a.ctx->UpdateSubresource(buffer, 0, nullptr, p.data(), a.width * 4u, 0);
    buffer->Release();
    a.chain->Present(0, 0);
    a.chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&buffer));
    D3D11_TEXTURE2D_DESC d{};
    buffer->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    ID3D11Texture2D * staging{};
    a.device->CreateTexture2D(&d, nullptr, &staging);
    if(staging)
      {
      a.ctx->CopyResource(staging, buffer);
      D3D11_MAPPED_SUBRESOURCE m{};
      if(SUCCEEDED(a.ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m)))
        {
        for(UINT y{}; y != a.height; ++y)
          std::memcpy(out.data() + y * a.width, static_cast<std::uint8_t const *>(m.pData) + y * m.RowPitch, a.width * 4u);
        a.ctx->Unmap(staging, 0);
        }
      staging->Release();
      }
    buffer->Release();
    return out;
    }


  ///\brief a 24-bit BMP of the picture (for a person to look at; tools/test.sh keeps them)
  auto write_bmp(char const * name, std::vector<std::uint32_t> const & p, UINT w, UINT h) -> void
    {
    FILE * const f{std::fopen(name, "wb")};
    if(not f)
      return;
    UINT const row{(w * 3u + 3u) & ~3u};
    std::uint32_t const size{54u + row * h};
    std::uint8_t head[54]{'B', 'M'};
    auto put32{[&](int at, std::uint32_t v) { std::memcpy(head + at, &v, 4); }};
    put32(2, size);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    head[26] = 1;
    head[28] = 24;
    put32(34, row * h);
    std::fwrite(head, 1, sizeof head, f);
    std::vector<std::uint8_t> line(row, 0u);
    for(UINT y{h}; y-- != 0;)
      {
      for(UINT x{}; x != w; ++x)
        {
        std::uint32_t const c{p[y * w + x]};
        line[x * 3u] = static_cast<std::uint8_t>(c >> 16);
        line[x * 3u + 1u] = static_cast<std::uint8_t>(c >> 8);
        line[x * 3u + 2u] = static_cast<std::uint8_t>(c);
        }
      std::fwrite(line.data(), 1, row, f);
      }
    std::fclose(f);
    }

  ///\brief a grid of white lines every 40 pixels on grey, to see the correction
  auto grid(UINT w, UINT h) -> std::vector<std::uint32_t>
    {
    std::vector<std::uint32_t> p(w * h, 0xff404040u);
    for(UINT y{}; y != h; ++y)
      for(UINT x{}; x != w; ++x)
        if(x % 40u < 2u or y % 40u < 2u)
          p[y * w + x] = 0xffe0e0e0u;
    return p;
    }

  auto channel(std::uint32_t c, int i) -> int { return static_cast<int>((c >> (8 * i)) & 255u); }

  auto same(std::vector<std::uint32_t> const & a, std::vector<std::uint32_t> const & b, int tolerance) -> bool
    {
    if(a.size() != b.size())
      return false;
    for(std::size_t i{}; i != a.size(); ++i)
      for(int k{}; k != 3; ++k)
        if(std::abs(channel(a[i], k) - channel(b[i], k)) > tolerance)
          return false;
    return true;
    }
  }  // namespace

auto main() -> int
  {
  SetEnvironmentVariableW(L"EDLOADER_CONFIG_DIR", L".");
  SetEnvironmentVariableW(L"EDLOADER_LOG_DIR", L".");
  // EDWIDE_TEST_NOHOOK (tools/test.sh's second run): wings_hook 0, nothing hooked, the picture as it was
  bool const no_hook{GetEnvironmentVariableW(L"EDWIDE_TEST_NOHOOK", nullptr, 0) != 0};
  write_ini(no_hook ? "wings = 1\nwings_hook = 0\nwings_angle = 30\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\n"
                    : "wings = 1\nwings_angle = 30\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\n");
  HMODULE const plugin{LoadLibraryW(L"edwide_screen.dll")};
  check(plugin != nullptr, "the plugin loaded");
  if(not plugin)
    return 1;
  auto const hooked{reinterpret_cast<int (*)()>(GetProcAddress(plugin, "edwide_screen_hooked"))};

  app_t a{};
  a.width = 960;
  a.height = 240;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof wc;
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"edwide_test";
  RegisterClassExW(&wc);
  HWND const window{CreateWindowExW(0, wc.lpszClassName, L"edwide test", WS_POPUP, 0, 0, 960, 240, nullptr, nullptr, wc.hInstance, nullptr)};
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferDesc.Width = a.width;
  sd.BufferDesc.Height = a.height;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = window;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;  // the picture kept after Present, to be read back
  // as the game through edloader: the device by the element's D3D11CreateDevice (on to the system's d3d11 here), the
  // swap chain by the device's factory, as Elite Dangerous makes it
  auto const create_device{reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(plugin, "D3D11CreateDevice"))};
  check(create_device != nullptr, "the plugin exports D3D11CreateDevice (a chain element)");
  HRESULT hr{create_device ? create_device(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &a.device, nullptr,
                                           &a.ctx)
                           : E_FAIL};
  if(SUCCEEDED(hr))
    {
    IDXGIDevice * dxgi_device{};
    IDXGIAdapter * adapter{};
    IDXGIFactory * factory{};
    a.device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgi_device));
    if(dxgi_device)
      dxgi_device->GetAdapter(&adapter);
    if(adapter)
      adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory));
    hr = factory ? factory->CreateSwapChain(a.device, &sd, &a.chain) : E_FAIL;
    for(IUnknown * u: std::initializer_list<IUnknown *>{factory, adapter, dxgi_device})
      if(u)
        u->Release();
    }
  check(SUCCEEDED(hr) and a.chain, "the test's swap chain made");
  if(FAILED(hr) or not a.chain)
    return 1;
  if(no_hook)
    {
    check(hooked and not hooked(), "wings_hook 0: nothing hooked");
    check(same(present(a), pattern(a.width, a.height), 0), "wings_hook 0: the picture as it was");
    std::printf(failures ? "FAILED (%d failure(s))\n" : "PASSED (%d failure(s))\n", failures);
    return failures ? 1 : 0;
    }
  check(hooked and hooked(), "the swap chain's Present hooked (through the device's DXGI factory)");

  std::vector<std::uint32_t> const plain{pattern(a.width, a.height)};
  std::vector<std::uint32_t> const turned{present(a)};
  // the middle untouched, bit for bit
  bool middle{true};
  for(UINT y{}; y != a.height; ++y)
    for(UINT x{240}; x != 720; ++x)
      middle = middle and turned[y * a.width + x] == plain[y * a.width + x];
  check(middle, "the middle monitor's part the same bits");
  // each side's pixel the colour of where its ray meets the picture (red by x, green by y), or black
  edwide::wings_layout_t const layout{.width = 960.f, .height = 240.f, .half_middle = 240.f, .bezel = 0.f,
                                      .plane = edwide::wings_plane(60.f, 240.f), .angle_deg = 30.f};
  int worst{}, checked{}, black_wrong{};
  for(UINT y{2}; y < a.height - 2; y += 7)
    for(UINT x{}; x != a.width; x += 5)
      {
      if(x >= 238 and x < 722)
        continue;
      auto const from{edwide::wings_source(layout, x + 0.5f, y + 0.5f)};
      std::uint32_t const c{turned[y * a.width + x]};
      if(not from)
        {
        if(channel(c, 0) + channel(c, 1) + channel(c, 2) != 0)
          ++black_wrong;
        continue;
        }
      if(from->x < 1.f or from->y < 1.f or from->x > a.width - 1.f or from->y > a.height - 1.f)
        continue;
      int const r{static_cast<int>(std::lround(from->x * 255.0 / a.width))};
      int const g{static_cast<int>(std::lround(from->y * 255.0 / a.height))};
      worst = std::max({worst, std::abs(channel(c, 0) - r), std::abs(channel(c, 1) - g)});
      ++checked;
      }
  std::printf("      sides: %d pixels checked, the worst %d of 255 off, %d not black where nothing is\n", checked, worst, black_wrong);
  check(checked > 500 and worst <= 3 and black_wrong == 0, "each side's pixel from where its ray meets the picture (wings_math.h)");
  // the sides narrowed towards the outer edge: a row below the middle rises there (as on the monitor turned towards you)
  auto const outer{edwide::wings_source(layout, 959.5f, 200.5f)};
  check(not outer or outer->y > 200.5f, "the outer edge takes a row further from the middle (the picture squeezed there)");

  // the eyes' distance set (80 cm, the middle 48 cm wide: 10 pixels a cm, the plane 800 pixels away): the sides as
  // seen from there, whatever the field of view
  write_ini("wings = 1\nwings_angle = 30\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\nwings_eye_cm = 80\nwings_center_cm = 48\n");
  (void)present(a);
  {
  std::vector<std::uint32_t> const seen{present(a)};
  edwide::wings_layout_t eye_layout{layout};
  eye_layout.plane = 800.f;
  int eye_worst{}, eye_checked{};
  for(UINT y{2}; y < a.height - 2; y += 7)
    for(UINT x{}; x < 238; x += 5)
      if(auto const from{edwide::wings_source(eye_layout, x + 0.5f, y + 0.5f)};
         from and from->x >= 1.f and from->y >= 1.f and from->x <= a.width - 1.f and from->y <= a.height - 1.f)
        {
        std::uint32_t const c{seen[y * a.width + x]};
        eye_worst = std::max({eye_worst, std::abs(channel(c, 0) - static_cast<int>(std::lround(from->x * 255.0 / a.width))),
                              std::abs(channel(c, 1) - static_cast<int>(std::lround(from->y * 255.0 / a.height)))});
        ++eye_checked;
        }
  check(eye_checked > 200 and eye_worst <= 3 and not same(seen, turned, 1), "wings_eye_cm: the sides as seen from the eyes' distance");
  }

  write_ini("wings = 1\nwings_angle = 0\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\n");
  (void)present(a);
  check(same(present(a), plain, 1), "angle 0: the picture as it was");
  write_ini("wings = 0\nwings_angle = 30\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\n");
  (void)present(a);
  check(same(present(a), plain, 0), "wings 0: the picture as it was");

  // another size (a window on the middle monitor only): untouched, said; the right size again: on again
  write_ini("wings = 1\nwings_angle = 30\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\n");
  (void)present(a);
  check(SUCCEEDED(a.chain->ResizeBuffers(1, 800, 240, DXGI_FORMAT_R8G8B8A8_UNORM, 0)), "ResizeBuffers to 800x240 not refused");
  a.width = 800;
  check(same(present(a), pattern(800, 240), 0), "800x240, not the layout's size: the picture as it was");
  check(read_log().find("off: the picture is 800x240, the settings expect a width of 960") != std::string::npos,
        "the log says why it is off");
  check(SUCCEEDED(a.chain->ResizeBuffers(1, 960, 240, DXGI_FORMAT_R8G8B8A8_UNORM, 0)), "ResizeBuffers back to 960x240 not refused");
  a.width = 960;
  check(not same(present(a), plain, 1), "960x240 again: the sides corrected again");
  check(read_log().find("on again: the picture 960x240") != std::string::npos, "the log says it is on again");

  // pictures for a person: the grid as the game gives it, corrected at 30 degrees
  std::vector<std::uint32_t> const lines{grid(a.width, a.height)};
  write_bmp("grid.bmp", lines, a.width, a.height);
  write_bmp("grid_30.bmp", present(a, &lines), a.width, a.height);
  // the calibration: at three angles, to see how the shapes on the sides change with it
  for(int angle: {20, 30, 40})
    {
    char ini[192], name[64];
    std::snprintf(ini, sizeof ini, "wings = 1\nwings_angle = %d\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\nwings_calibrate = 1\n", angle);
    write_ini(ini);
    (void)present(a, &lines);
    std::snprintf(name, sizeof name, "calibrate_%d.bmp", angle);
    write_bmp(name, present(a, &lines), a.width, a.height);
    }
  // wings_calibrate 0 again: nothing of it left
  write_ini("wings = 1\nwings_angle = 30\nwings_center_px = 480\nwings_side_px = 240\nwings_fov = 60\nwings_calibrate = 0\n");
  (void)present(a);
  check(same(present(a), turned, 0), "wings_calibrate 0: the corrected picture without the shapes");

  // EDWIDE_BENCH=<width>x<height> (tools/bench.sh): the correction's time on the graphics card at that size, the
  // frame's time with it on less the time with it off, the median of 100 frames each
  wchar_t bench[32]{};
  if(GetEnvironmentVariableW(L"EDWIDE_BENCH", bench, 32) != 0)
    {
    UINT bw{}, bh{};
    if(std::swscanf(bench, L"%ux%u", &bw, &bh) == 2 and bw >= 64u and bh >= 64u)
      {
      a.chain->ResizeBuffers(1, bw, bh, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
      a.width = bw;
      a.height = bh;
      D3D11_QUERY_DESC q{};
      ID3D11Query * disjoint{};
      ID3D11Query * begin{};
      ID3D11Query * end{};
      q.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
      a.device->CreateQuery(&q, &disjoint);
      q.Query = D3D11_QUERY_TIMESTAMP;
      a.device->CreateQuery(&q, &begin);
      a.device->CreateQuery(&q, &end);
      auto const median{[&]() -> double
                          {
                            std::vector<double> times;
                            for(int i{}; i != 110; ++i)
                              {
                              a.ctx->Begin(disjoint);
                              a.ctx->End(begin);
                              a.chain->Present(0, 0);
                              a.ctx->End(end);
                              a.ctx->End(disjoint);
                              D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
                              UINT64 t0{}, t1{};
                              while(a.ctx->GetData(disjoint, &dj, sizeof dj, 0) != S_OK)
                                Sleep(0);
                              while(a.ctx->GetData(begin, &t0, sizeof t0, 0) != S_OK)
                                Sleep(0);
                              while(a.ctx->GetData(end, &t1, sizeof t1, 0) != S_OK)
                                Sleep(0);
                              if(i >= 10 and not dj.Disjoint and dj.Frequency and t1 >= t0)
                                times.push_back(double(t1 - t0) * 1000. / double(dj.Frequency));
                              }
                            std::sort(times.begin(), times.end());
                            return times.empty() ? 0. : times[times.size() / 2u];
                          }};
      char on_ini[256];
      std::snprintf(on_ini, sizeof on_ini, "wings = 1\nwings_angle = 30\nwings_center_px = %u\nwings_fov = 58.2\n", bw / 3u);
      write_ini(on_ini);
      double const with{median()};
      write_ini("wings = 0\n");
      double const without{median()};
      std::printf("bench gpu: %ux%u: a frame %.3f ms with the correction, %.3f ms without, the correction %.3f ms\n", bw, bh, with, without,
                  with - without);
      for(IUnknown * u: std::initializer_list<IUnknown *>{end, begin, disjoint})
        if(u)
          u->Release();
      }
    }

  a.ctx->Release();
  a.device->Release();
  a.chain->Release();
  DestroyWindow(window);
  std::printf(failures ? "FAILED (%d failure(s))\n" : "PASSED (%d failure(s))\n", failures);
  return failures ? 1 : 0;
  }
