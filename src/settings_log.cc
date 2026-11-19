// edwide - what edwide_screen's parts share: the log, the settings (edwide_screen.ini) and the game's field of view.
#include "runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace edwide
  {
  HMODULE self{};
  std::wstring config_dir, log_dir;
  HANDLE log_file{INVALID_HANDLE_VALUE};
  SRWLOCK log_lock = SRWLOCK_INIT;

  namespace
    {
    ///\brief the room a log line is made in, and an output iterator that stops at its end
    struct cut_buffer_t
      {
      char * at;
      char * end;
      };

    struct cut_out_t
      {
      using difference_type = std::ptrdiff_t;
      cut_buffer_t * b;

      auto operator*() noexcept -> cut_out_t & { return *this; }

      auto operator=(char c) noexcept -> cut_out_t &
        {
        if(b->at != b->end)
          *b->at++ = c;
        return *this;
        }

      auto operator++() noexcept -> cut_out_t & { return *this; }

      auto operator++(int) noexcept -> cut_out_t { return *this; }
      };
    }  // namespace

  auto log_vline(std::string_view fmt, std::format_args args) noexcept -> void
    {
    if(log_file == INVALID_HANDLE_VALUE)
      return;
    SYSTEMTIME t;
    GetSystemTime(&t);
    char buf[1024];
    cut_buffer_t room{buf, buf + sizeof buf - 1};  // one byte kept for the line's end
    try
      {
      std::format_to(cut_out_t{&room}, "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                     t.wMilliseconds);
      std::vformat_to(cut_out_t{&room}, fmt, args);
      }
    catch(...)
      {
      constexpr std::string_view failed{" (log: the rest of this line could not be formatted)"};
      for(char const c: failed)
        if(room.at != room.end)
          *room.at++ = c;
      }
    *room.at++ = '\n';
    DWORD written{};
    exclusive_lock_t const hold{log_lock};
    WriteFile(log_file, buf, static_cast<DWORD>(room.at - buf), &written, nullptr);
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
            log_line("settings: '{} = {}' cannot be read; {} stays as it was", key, value, key);
          else
            s.*k.field = f;
          }
      if(not known and not key.empty())
        log_line("settings: no setting '{}' (a typo?): ignored", key);
      }
    s.zoom = std::clamp(s.zoom, 0.25f, 4.f);
    s.curve = std::clamp(s.curve, 0.f, 1.f);
    s.angle = std::clamp(s.angle, -80.f, 80.f);
    settings = s;
    log_line("settings: wings {:g}, angle {:g}, centre {:g} px, side {:g} px, bezel {:g} px, eye {:g} cm, centre {:g} cm, fov {:g}, zoom {:g}, curve {:g}, "
             "offset {:g}, calibrate {:g}",
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
    log_line("settings: {} made, all its lines comments", wide(ini_path));
    }

  ///\brief the game's vertical field of view from its Settings.xml (`<FOV>`), read again when the file changes
  auto read_game_fov() -> void
    {
    std::string const text{read_text(game_settings_path)};
    auto const at{text.find("<FOV>")};
    float const fov{at == std::string::npos ? 0.f : std::strtof(text.c_str() + at + 5, nullptr)};
    if(fov != game_fov)
      log_line("game: the field of view {:g} degrees ({})", double(fov), wide(game_settings_path));
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
  }  // namespace edwide
