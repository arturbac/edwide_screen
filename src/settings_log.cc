// edwide - what edwide_screen's parts share: the log, the settings (edwide_screen.ini, read, completed and watched by
// edplugin_common's ini module from the table below) and the game's field of view.
#include "runtime.h"

#include <edplugin_common/ini/ini.h>
#include <edplugin_common/settings/settings_doc.h>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

namespace edwide
  {
  HMODULE self{};
  std::wstring config_dir, log_dir;
  edplugin_common::live_settings_t<settings_t> live_settings;
  std::wstring game_settings_path;
  float game_fov{};

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

    static_assert(std::size(groups) == static_cast<std::size_t>(group_e::diagnostics) + 1, "a name for each group");

    using edplugin_common::setting_read_e;
    using setting_t = edplugin_common::ini_setting_t<settings_t, group_e>;

    constexpr auto live{setting_read_e::live};
    constexpr auto at_start{setting_read_e::at_start};

    // every setting once, in the order a new ini gets them: the ini's reading, its comments and doc/settings.md's rows
    // come from here (tools/doc.sh checks the ini against test/data and the doc against doc/settings.md). The range
    // also goes into the note of a line that cannot be read
    constexpr setting_t settings_rows[]{
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
       "only without wings_eye_cm: the game's vertical field of view, degrees; 0 = read from the game's Settings.xml"sv,
       "used unless wings_eye_cm and wings_center_cm are both above 0"sv},
      {"wings_zoom"sv, &settings_t::zoom, group_e::look, live, "0.25..4 (clamped)"sv,
       "the sides' size: above 1 larger (less black at the corners, more cut off)"sv},
      {"wings_curve"sv, &settings_t::curve, group_e::look, live, "0..1 (clamped)"sv, "0 flat side monitors .. 1 curved ones"sv},
      {"wings_offset_y"sv, &settings_t::offset_y, group_e::look, live, ""sv, "the sides moved up (-) or down (+), pixels"sv},
      {"wings_calibrate"sv, &settings_t::calibrate, group_e::diagnostics, live, "above 0.5 = on"sv,
       "1 = shapes to set the angle by: circles, squares and rows across the monitors' edges, round, square and straight from where you sit when wings_angle is right"sv},
    };

    // the keys taken out: their lines passed over with a note of their own, once a session
    constexpr edplugin_common::ini_withdrawn_t withdrawn[]{
      {"wings_guides"sv, "wings_calibrate"sv},
    };

    constexpr edplugin_common::ini_table_t<settings_t, group_e> table{settings_rows, withdrawn, edplugin_common::ini_notes_e::about,
                                                                      edplugin_common::bad_item_e::reject_line,
                                                                      edplugin_common::hex_zero_e::refused};

    constexpr edplugin_common::ini_texts_t ini_texts{
      "; edwide_screen: the sides of the game's picture laid over side monitors set at an angle (README.md)\n"
      "; a line without ';' sets a value; read again within a second of a change\n"sv,
      "; settings this file did not name yet, added by edwide_screen\n"sv};

    // the ini's watch and what was said of it this session: the start's thread, then the render thread (Present)
    edplugin_common::ini_watch_t ini_watch;
    edplugin_common::ini_watch_t game_settings_watch;
    edplugin_common::ini_told_t told;

    ///\brief the values held to their ranges after a read (the table's "(clamped)")
    auto clamp(settings_t & s) noexcept -> void
      {
      s.zoom = std::clamp(s.zoom, 0.25f, 4.f);
      s.curve = std::clamp(s.curve, 0.f, 1.f);
      s.angle = std::clamp(s.angle, -80.f, 80.f);
      }

    auto log_completion(edplugin_common::ini_complete_t const & done) -> void
      {
      using edplugin_common::ini_complete_e;
      switch(done.status)
        {
        case ini_complete_e::made:
          log_line("settings: {} made, all its lines comments", wide(ini_watch.path()));
          break;
        case ini_complete_e::added:
          log_line("settings: {} added to {} as comments", done.count, wide(ini_watch.path()));
          break;
        case ini_complete_e::write_failed:
          log_line("settings: {} not written whole (error {}); {}", wide(ini_watch.path()), done.error,
                   done.undone ? "nothing of it left" : "a part of it left");
          break;
        case ini_complete_e::no_memory:
          log_line("settings: the lines {} lacks could not be made (no memory); nothing added", wide(ini_watch.path()));
          break;
        case ini_complete_e::nothing:
        case ini_complete_e::unreadable:  // said by the read after it
        case ini_complete_e::utf16:
          break;
        }
      }

    ///\brief the ini read into `s` (the defaults) and held to its ranges; false when nothing was read: no file, one that
    /// cannot be read (said once until it is read) or UTF-16 (said once)
    auto read_into(settings_t & s, edplugin_common::ini_read_t & r) -> bool
      {
      using edplugin_common::ini_file_e;
      switch(edplugin_common::read_ini_file(ini_watch.path(), table, s, r, told))
        {
        case ini_file_e::read:
          ini_watch.readable();
          clamp(s);
          return true;
        case ini_file_e::none:
          return false;
        case ini_file_e::unreadable:
          if(ini_watch.unreadable())
            log_line("settings: {} could not be read; the settings stay as they were", wide(ini_watch.path()));
          return false;
        case ini_file_e::utf16:
          if(told.first("utf16"))
            log_line("settings: {} is UTF-16: neither read nor added to (save it as UTF-8); the settings stay as they were",
                     wide(ini_watch.path()));
          return false;
        }
      return false;
      }

    auto log_settings(std::vector<std::string> const & notes, settings_t const & s) -> void
      {
      for(std::string const & note: notes)
        log_line("settings: {}", note);
      log_line("settings: wings {:g}, angle {:g}, centre {:g} px, side {:g} px, bezel {:g} px, eye {:g} cm, centre {:g} cm, fov {:g}, zoom {:g}, curve {:g}, "
               "offset {:g}, calibrate {:g}",
               double(s.wings), double(s.angle), double(s.centre_px), double(s.side_px), double(s.bezel_px), double(s.eye_cm),
               double(s.centre_cm), double(s.fov), double(s.zoom), double(s.curve), double(s.offset_y), double(s.calibrate));
      }

    ///\brief the ini read again while the game runs: its live settings published, a start-only one changed said
    auto read_ini_again() -> void
      {
      try
        {
        settings_t s{};
        edplugin_common::ini_read_t r;
        if(not read_into(s, r))
          return;
        edplugin_common::at_start_changes(table, s, r, live_settings.get(), told, r.notes);
        live_settings.publish(s, table);
        log_settings(r.notes, s);
        }
      catch(...)
        {
        log_line("settings: the ini not read again (no memory); the settings stay as they were");
        }
      }

    ///\brief the game's vertical field of view from its Settings.xml (`<FOV>`), read again when the file changes
    auto read_game_fov() -> void
      {
      std::optional<std::string> const read{read_text(game_settings_path)};
      if(not read)
        {
        // read again at the next look (its time forgotten), said once until it is read
        if(game_settings_watch.unreadable())
          log_line("game: {} could not be read; the field of view stays {:g} degrees", wide(game_settings_path), double(game_fov));
        return;
        }
      game_settings_watch.readable();
      std::string const & text{*read};
      auto const at{text.find("<FOV>")};
      float const fov{at == std::string::npos ? 0.f : std::strtof(text.c_str() + at + 5, nullptr)};
      if(fov != game_fov)
        log_line("game: the field of view {:g} degrees ({})", double(fov), wide(game_settings_path));
      game_fov = fov;
      }
    }  // namespace

  auto start_ini(std::wstring path) -> void
    {
    ini_watch.set_path(std::move(path));
    // the values written are the ones edwide_screen runs on without the file: its defaults
    settings_t s{};
    log_completion(edplugin_common::complete_ini(ini_watch.path(), table, s, ini_texts));
    ini_watch.mark_seen();
    edplugin_common::ini_read_t r;
    if(read_into(s, r))
      log_settings(r.notes, s);
    live_settings.start(s);
    }

  auto write_settings_doc() -> bool
    {
    settings_t const d{};
    edplugin_common::ini_doc_rows_t<settings_t, group_e> const rows{table, d};
    constexpr edplugin_common::settings_doc_t doc{
      "edwide_screen settings"sv,
      "edwide_screen from its table of settings (src/settings_log.cc, `EDWIDE_SETTINGS_DOC`)"sv,
      "In `edwide_screen.ini`, in `edloader\\config` (beside the dll without edloader); edwide_screen makes it at the "
      "game's first start with every setting as a comment, so the correction is off until it is set, and adds the "
      "settings a newer version brings at its end. A line without `;` sets a value (`;` and `#` start a comment, also "
      "after a value).\n"
      "\n"
      "- **Read while the game runs**: edwide_screen looks at the ini when the game shows a frame, at most once a second, "
      "and reads it again when the file was saved since; the log has the values read (`settings: ...`). A line taken "
      "out, or made a comment, gives the default back; the file taken away leaves the settings as they are.\n"
      "- **Read at the game's start**: a change takes a new start of the game (the log says so once).\n"
      "- **Range**: the values that make sense; *(clamped)* = edwide_screen holds the value to it, elsewhere it does not "
      "check; empty = any value.\n"sv,
      groups};
    edplugin_common::settings_doc_result_t const r{edplugin_common::write_settings_doc(L"EDWIDE_SETTINGS_DOC", doc, rows.rows())};
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

  auto start_game_settings(std::wstring path) -> void { game_settings_watch.set_path(std::move(path)); }

  auto look_at_files() -> void
    {
    ULONGLONG const now{GetTickCount64()};
    switch(ini_watch.look(now))
      {
      case edplugin_common::ini_look_e::changed:
        read_ini_again();
        break;
      case edplugin_common::ini_look_e::gone:
        log_line("settings: {} taken away; the settings stay as they are until it is back", wide(ini_watch.path()));
        break;
      case edplugin_common::ini_look_e::not_due:
      case edplugin_common::ini_look_e::same:
        break;
      }
    // taken away: the field of view as it was
    if(game_settings_watch.look(now) == edplugin_common::ini_look_e::changed)
      read_game_fov();
    }
  }  // namespace edwide
