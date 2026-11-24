// edwide - what edwide_screen's parts share: the log, the settings (edwide_screen.ini) and the game's field of view.
#pragma once

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <edplugin_common/core/lock.h>
#include <edplugin_common/file/file.h>
#include <edplugin_common/ini/live.h>
#include <edplugin_common/log/log.h>
#include <edplugin_common/paths/paths.h>
#include <edplugin_common/text/format.h>

#include <atomic>
#include <string>
#include <format>
#include <optional>
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
  ///\brief the settings as the game's threads see them (edplugin_common's live.h): the start-only ones (wings_hook)
  /// from any thread, the others on the render thread, where look_at_files publishes them
  extern edplugin_common::live_settings_t<settings_t> live_settings;
  extern std::wstring game_settings_path;
  extern float game_fov;

  using edplugin_common::as_unsigned;
  using edplugin_common::exclusive_lock_t;
  using edplugin_common::shared_lock_t;
  using edplugin_common::wide;
  using edplugin_common::wide_t;
  // the files and folders are edplugin_common's
  using edplugin_common::read_text;
  using edplugin_common::write_time;
  // the log is edplugin_common's: edwide_screen.log, cut at 2 KB a line, the last session's set aside in the folder
  // edwide_screen beside it
  using edplugin_common::log_is_open;
  using edplugin_common::log_line;
  using edplugin_common::log_open;
  using edplugin_common::log_vline;

  ///\brief std::format into `size` characters at `buf`, as snprintf did: cut at the end, NUL-terminated; the characters
  /// written (without the NUL). Never thrown: a text that cannot be made is empty, and logged
  template<typename char_t, typename... args_t>
  auto format_into(char_t * buf, std::size_t size, std::type_identity_t<std::basic_format_string<char_t, args_t...>> fmt,
                   args_t &&... args) noexcept -> std::size_t
    {
    std::optional<std::size_t> const n{edplugin_common::try_format_into(buf, size, fmt, std::forward<args_t>(args)...)};
    if(not n)
      log_vline("format: a text could not be made; left empty", std::make_format_args());
    return n.value_or(0);
    }

  template<typename char_t, std::size_t size, typename... args_t>
  auto format_into(char_t (&buf)[size], std::type_identity_t<std::basic_format_string<char_t, args_t...>> fmt, args_t &&... args) noexcept
    -> std::size_t
    {
    return format_into(&buf[0], size, fmt, std::forward<args_t>(args)...);
    }

  ///\brief the settings in use, through live_settings
  [[nodiscard]]
  inline auto settings() noexcept -> settings_t const &
    {
    return live_settings.get();
    }

  ///\brief the start's part of the ini (edplugin_common's ini.h): completed with the settings it does not name (a new
  /// one with every setting as a comment), read, the notes logged, live_settings started
  auto start_ini(std::wstring path) -> void;
  ///\brief doc/settings.md from the table of settings, to the file EDWIDE_SETTINGS_DOC names (nothing without it); false
  /// when it was asked and not written (in the log)
  [[nodiscard]]
  auto write_settings_doc() -> bool;
  ///\brief the game's Settings.xml watched by look_at_files (its first look reads it)
  auto start_game_settings(std::wstring path) -> void;
  ///\brief the ini and the game's Settings.xml read again when they changed, looked at once a second at most; on the
  /// render thread
  auto look_at_files() -> void;
  }  // namespace edwide
