// edwide - what edwide_screen's parts share: the log, the settings (edwide_screen.ini) and the game's field of view.
#include "runtime.h"

#include <edplugin_common/settings/settings_doc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

namespace edwide
  {
  HMODULE self{};
  std::wstring config_dir, log_dir;

  namespace
    {
    ///\brief the topics of doc/settings.md, in its order
    constexpr std::string_view groups[]{"Turning it on"sv, "The monitors"sv, "The distance"sv, "The look"sv,
                                        "Calibration and diagnostics"sv};

    enum struct group_e : std::size_t
      {
      turning_on,
      monitors,
      distance,
      look,
      diagnostics
      };

    using edplugin_common::setting_read_e;

    ///\brief one setting: read from the ini by its field, written to a new ini by its help and default, and listed in
    /// doc/settings.md with its group, when it is read, its range and, after the help, `doc_note`
    struct setting_t
      {
      std::string_view name;
      float settings_t::* field;
      group_e group;
      setting_read_e read;
      ///\brief the values that make sense, "(clamped)" where read_ini holds the value to them; empty = any
      std::string_view range;
      std::string_view help;
      ///\brief for doc/settings.md only, after the help and "; " (the ini's comments stay as they are)
      std::string_view doc_note{};
      };

    constexpr auto live{setting_read_e::live};
    constexpr auto at_start{setting_read_e::at_start};

    // every setting once, in the order a new ini gets them: read_ini takes its names from here, make_ini its comments
    // and doc/settings.md its rows (tools/doc.sh checks the ini against test/data and the doc against doc/settings.md)
    constexpr setting_t settings_table[]{
      {"wings_hook"sv, &settings_t::hook, group_e::diagnostics, at_start, "0.5 or above = on"sv,
       "read when the game starts: 1 = the hook in place; 0 = nothing hooked (the calls only passed on), to tell whether another mod is disturbed by it"sv},
      {"wings"sv, &settings_t::wings, group_e::turning_on, live, "0.5 or above = on"sv,
       "1 = the sides laid over monitors set at an angle; 0 = off"sv},
      {"wings_angle"sv, &settings_t::angle, group_e::monitors, live, "-80..80 (clamped)"sv,
       "the side monitors' angle towards you, degrees (0 = in one line with the middle)"sv},
      {"wings_center_px"sv, &settings_t::centre_px, group_e::monitors, live, "0 or less = off"sv,
       "the middle monitor's width in pixels of the game's picture; 0 = not set (off)"sv, "e.g. 3000 of a picture 9000 wide"sv},
      {"wings_side_px"sv, &settings_t::side_px, group_e::monitors, live,
       "above 0: the picture's width must be wings_center_px + 2 x (wings_side_px + wings_bezel_px)"sv,
       "each side monitor's width in pixels; 0 = the rest of the picture (any width)"sv,
       "when set, a picture of another width is left alone"sv},
      {"wings_bezel_px"sv, &settings_t::bezel_px, group_e::monitors, live, ""sv,
       "pixels of the picture behind the bezels between the middle and a side (0 = none)"sv,
       "only when the graphics driver adds them"sv},
      {"wings_eye_cm"sv, &settings_t::eye_cm, group_e::distance, live, "used when it and wings_center_cm are above 0"sv,
       "your eyes' distance from the middle monitor, cm (with wings_center_cm: the sides as seen from there; 0 = from the field of view)"sv},
      {"wings_center_cm"sv, &settings_t::centre_cm, group_e::distance, live, "used when it and wings_eye_cm are above 0"sv,
       "the middle monitor's picture width, cm (0 = from the field of view)"sv},
      {"wings_fov"sv, &settings_t::fov, group_e::distance, live, "used above 0; works between 1 and 179"sv,
       "only without wings_eye_cm: the game's vertical field of view, degrees; 0 = read from the game's Settings.xml"sv},
      {"wings_zoom"sv, &settings_t::zoom, group_e::look, live, "0.25..4 (clamped)"sv,
       "the sides' size: above 1 larger (less black at the corners, more cut off)"sv},
      {"wings_curve"sv, &settings_t::curve, group_e::look, live, "0..1 (clamped)"sv, "0 flat side monitors .. 1 curved ones"sv},
      {"wings_offset_y"sv, &settings_t::offset_y, group_e::look, live, ""sv, "the sides moved up (-) or down (+), pixels"sv},
      {"wings_calibrate"sv, &settings_t::calibrate, group_e::diagnostics, live, "above 0.5 = on"sv,
       "1 = shapes to set the angle by: circles, squares and rows across the monitors' edges, round, square and straight from where you sit when wings_angle is right"sv},
    };
    }  // namespace

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
    std::optional<std::string> const read{read_text(ini_path)};
    static bool told_unreadable{};
    if(not read)
      {
      // read again at the next look (its time forgotten), said once until it is read
      ini_written = 0;
      if(not told_unreadable)
        log_line("settings: {} could not be read; the settings stay as they were", wide(ini_path));
      told_unreadable = true;
      return;
      }
    told_unreadable = false;
    std::string const & text{*read};
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
      for(setting_t const & k: settings_table)
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
    for(setting_t const & k: settings_table)
      {
      char line[256];
      format_into(line, "; {}\r\n;{} = {:g}\r\n", k.help, k.name, double(d.*k.field));
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

  auto write_settings_doc() -> bool
    {
    settings_t const d{};
    std::vector<std::string> values;  // the rows' views stay on these
    values.reserve(std::size(settings_table));
    std::vector<std::string> abouts;
    abouts.reserve(std::size(settings_table));
    std::vector<edplugin_common::setting_row_t> rows;
    rows.reserve(std::size(settings_table));
    for(setting_t const & setting: settings_table)
      {
      char value[32];
      format_into(value, "{:g}", double(d.*setting.field));
      values.emplace_back(value);
      std::string about{setting.help};
      if(not setting.doc_note.empty())
        (about += "; "sv) += setting.doc_note;
      abouts.push_back(std::move(about));
      rows.push_back({setting.name, values.back(), static_cast<std::size_t>(setting.group), setting.read, setting.range, abouts.back()});
      }
    constexpr edplugin_common::settings_doc_t doc{
      "edwide_screen settings"sv,
      "edwide_screen from its table of settings (src/settings_log.cc, `EDWIDE_SETTINGS_DOC`)"sv,
      "In `edwide_screen.ini`, in `edloader\\config` (beside the dll without edloader); edwide_screen makes it at the "
      "game's first start with every setting as a comment, so the correction is off until it is set. A line without `;` "
      "sets a value (`;` and `#` start a comment).\n"
      "\n"
      "- **Read while the game runs**: edwide_screen looks at the ini when the game shows a frame, at most once a second, "
      "and reads it again when the file was saved since; the log has the values read (`settings: ...`).\n"
      "- **Read at the game's start**: a change takes a new start of the game.\n"
      "- **Range**: the values that make sense; *(clamped)* = edwide_screen holds the value to it, elsewhere it does not "
      "check.\n"sv,
      groups};
    edplugin_common::settings_doc_result_t const r{edplugin_common::write_settings_doc(L"EDWIDE_SETTINGS_DOC", doc, rows)};
    switch(r.status)
      {
      case edplugin_common::settings_doc_status_e::not_asked:
        return true;
      case edplugin_common::settings_doc_status_e::written:
        log_line("settings: the settings' doc written to {}", wide(r.path));
        return true;
      case edplugin_common::settings_doc_status_e::path_too_long:
        log_line("settings: the settings' doc not written: its path is 260 characters or longer ({}...)", wide(r.path));
        break;
      case edplugin_common::settings_doc_status_e::invalid:
        log_line("settings: the settings' doc not written: {}", edplugin_common::settings_doc_error_name(r.error));
        break;
      case edplugin_common::settings_doc_status_e::cannot_open:
        log_line("settings: the settings' doc {} cannot be made (error {})", wide(r.path), r.system_error);
        break;
      case edplugin_common::settings_doc_status_e::write_failed:
        log_line("settings: writing the settings' doc {} failed (error {})", wide(r.path), r.system_error);
        break;
      }
    return false;
    }

  ///\brief the game's vertical field of view from its Settings.xml (`<FOV>`), read again when the file changes
  auto read_game_fov() -> void
    {
    std::optional<std::string> const read{read_text(game_settings_path)};
    static bool told_unreadable{};
    if(not read)
      {
      // read again at the next look (its time forgotten), said once until it is read
      game_settings_written = 0;
      if(not told_unreadable)
        log_line("game: {} could not be read; the field of view stays {:g} degrees", wide(game_settings_path), double(game_fov));
      told_unreadable = true;
      return;
      }
    told_unreadable = false;
    std::string const & text{*read};
    auto const at{text.find("<FOV>")};
    float const fov{at == std::string::npos ? 0.f : std::strtof(text.c_str() + at + 5, nullptr)};
    if(fov != game_fov)
      log_line("game: the field of view {:g} degrees ({})", double(fov), wide(game_settings_path));
    game_fov = fov;
    }

  ///\brief the ini read now when it is there, its time noted for look_at_files
  auto read_ini_now() -> void
    {
    ini_written = write_time(ini_path);
    if(ini_written != 0)
      read_ini();
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
