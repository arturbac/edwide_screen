// edwide - what edwide_screen's parts share: the log, the settings (edwide_screen.ini) and the game's field of view.
#pragma once

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <atomic>
#include <string>

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

  auto log_line(char const * fmt, ...) noexcept -> void;
  ///\brief a folder edloader hands its plugins (EDLOADER_CONFIG_DIR, EDLOADER_LOG_DIR); without edloader, the dll's own
  auto directory_from(wchar_t const * variable) -> std::wstring;
  ///\brief the ini with every setting as a comment, when there is none
  auto make_ini() -> void;
  auto look_at_files() -> void;
  }  // namespace edwide
