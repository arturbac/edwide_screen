// edwide - what edwide_screen's parts share: the log, the settings (edwide_screen.ini) and the game's field of view.
#pragma once

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <atomic>
#include <string>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>
#include <algorithm>

namespace edwide
  {
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


  extern HMODULE self;
  extern std::wstring config_dir, log_dir;
  extern HANDLE log_file;
  extern settings_t settings;
  extern std::wstring ini_path;
  extern std::wstring game_settings_path;
  extern float game_fov;

  ///\brief an SRW lock held exclusively for a scope: let go on every way out of it
  class exclusive_lock_t
    {
    SRWLOCK & lock_;

  public:
    explicit exclusive_lock_t(SRWLOCK & lock) noexcept : lock_{lock} { AcquireSRWLockExclusive(&lock_); }

    ~exclusive_lock_t() { ReleaseSRWLockExclusive(&lock_); }

    exclusive_lock_t(exclusive_lock_t const &) = delete;
    auto operator=(exclusive_lock_t const &) -> exclusive_lock_t & = delete;
    };

  ///\brief an SRW lock held shared for a scope
  class shared_lock_t
    {
    SRWLOCK & lock_;

  public:
    explicit shared_lock_t(SRWLOCK & lock) noexcept : lock_{lock} { AcquireSRWLockShared(&lock_); }

    ~shared_lock_t() { ReleaseSRWLockShared(&lock_); }

    shared_lock_t(shared_lock_t const &) = delete;
    auto operator=(shared_lock_t const &) -> shared_lock_t & = delete;
    };

  ///\brief a wide string in a log line, written as UTF-8 (std::format takes no wide string into a narrow line)
  struct wide_t
    {
    std::wstring_view text;
    };

  [[nodiscard]]
  inline auto wide(std::wstring_view text) noexcept -> wide_t
    {
    return {text};
    }

  [[nodiscard]]
  inline auto wide(wchar_t const * text) noexcept -> wide_t
    {
    return {text ? std::wstring_view{text} : std::wstring_view{L"(null)"}};
    }

  ///\brief a value for a hexadecimal field: a signed one (HRESULT, LONG) as its bits, as printf's %X showed it
  template<typename value_t>
  [[nodiscard]]
  constexpr auto as_unsigned(value_t value) noexcept
    {
    if constexpr(std::is_enum_v<value_t>)
      return as_unsigned(std::to_underlying(value));
    else if constexpr(std::is_integral_v<value_t> and std::is_signed_v<value_t>)
      return static_cast<std::make_unsigned_t<value_t>>(value);
    else
      return value;
    }

  ///\brief one line, time-stamped (UTC), cut to 1 KB, written at once under the log's lock; a line that cannot be made
  /// is logged as such (never thrown)
  auto log_vline(std::string_view fmt, std::format_args args) noexcept -> void;

  ///\brief log_vline with the format checked against its arguments when compiled
  template<typename... args_t>
  auto log_line(std::format_string<args_t...> fmt, args_t &&... args) noexcept -> void
    {
    log_vline(fmt.get(), std::make_format_args(args...));
    }
  ///\brief a folder edloader hands its plugins (EDLOADER_CONFIG_DIR, EDLOADER_LOG_DIR); without edloader, the dll's own
  auto directory_from(wchar_t const * variable) -> std::wstring;
  ///\brief the ini with every setting as a comment, when there is none
  auto make_ini() -> void;
  auto look_at_files() -> void;
  }  // namespace edwide

///\brief a wide string (edwide::wide) in a log line as UTF-8, by pieces through a buffer on the stack
template<>
struct std::formatter<edwide::wide_t, char> : std::formatter<std::string_view, char>
  {
  auto format(edwide::wide_t const & w, std::format_context & ctx) const -> std::format_context::iterator
    {
    auto out{ctx.out()};
    std::wstring_view rest{w.text};
    char buf[512];
    while(not rest.empty())
      {
      std::size_t len{std::min<std::size_t>(rest.size(), 120)};
      if(len < rest.size() and rest[len - 1] >= 0xD800 and rest[len - 1] <= 0xDBFF)
        --len;  // a surrogate pair stays whole
      int const n{WideCharToMultiByte(CP_UTF8, 0, rest.data(), static_cast<int>(len), buf, static_cast<int>(sizeof buf), nullptr, nullptr)};
      for(int i{}; i < n; ++i)
        *out++ = buf[i];
      rest.remove_prefix(len);
      }
    return out;
    }
  };
