# Changelog

## Unreleased

- `edwide_screen.ini` is read, completed and watched by edplugin_common's module `ini` (1669a91), by the same rules
  as edupscale's and edworld's ini. A value that works today works the same:
  - an ini that is there gets the settings it does not name yet added at its end as comments (before, it was never
    added to); the player's lines stay byte for byte, and the lines added take the file's own line ends;
  - the log's notes name the line (`settings: line N: ...`); a line without `=` is said too (before, passed over
    without a word);
  - `wings_guides`, taken out for `wings_calibrate` before the first release, is said once a session as `no longer a
    setting (now wings_calibrate)` (before, `a typo?`);
  - the ini taken away while the game runs leaves the settings as they are, said once (before, the defaults until
    it was back); `wings_hook` changed while the game runs is said once (it is read at the game's start);
  - an ini saved as UTF-16 is neither read nor added to, said once in the log (save it as UTF-8);
  - a new ini has an empty line before each setting's comment.
- Lines that now take effect: the first line of an ini saved with a UTF-8 byte order mark (before, its key was taken
  for no setting).
- The game's `Settings.xml` taken away while the game runs keeps the field of view as it was (before, 0 until it was
  back).
- doc/developers.md moved out of the repository, to the developers' notes kept apart from the plugins; the README no
  longer links it.
- After a fault in the correction the device's objects are kept (before, let go: another thread's Present still in
  the correction could have used them); the log's line says `the picture let go (the device's objects kept)`.
- `build_info` tells a fault from the setting: `start off` (a fault in the start), `hook off` (a fault while hooking;
  a swap chain of a class hooked before keeps its correction), `hook 0` only for `wings_hook = 0` (before, all three said
  `hook 0`).
- A new reason of the same state at the same size is a line of its own. The correction says `off: <why>` where it
  stopped without a word: no device or picture from the swap chain, a
  context without `ID3D11DeviceContext1` (before, the log said `on` and nothing was drawn).
- A new `edwide_screen.ini` that cannot be made (no folder, no right to write) is said in the log with the error
  (before, nothing); one made by another between the look and the write is kept, as before. Its lines go straight into
  the text, so a long help is never cut. The ini byte for byte as before.
- `tools/test.sh`: three runs with the test's fault (start, hook, correction) through the export
  `edwide_screen_test_fault`, for the test only.
- `make_gpu`'s `ID3D11Device1` held in the frame's record, so a fault does not leave it referenced.

- The settings are one table in the code: the ini made from it, read by it, and `doc/settings.md` made from it (each
  setting's default, range and whether a change applies at once or at the next start); `doc/users.md` links it in
  place of its own table. The ini and the log's settings lines stay as they were. `tools/doc.sh` checks the ini, the
  log's lines on wrong ones and the doc with no device and no drawing; `tools/test.sh` runs it last.
- Faults are caught by edplugin_common's guard: in the correction, in putting the hooks in place (now also the Present
  hook of a swap chain made through the hooked factory, which before had no handler) and in the start. A fault in the
  correction or the hooks is logged with its place (`<module>+0x<offset>`; a C++ exception with its text) in two
  lines, `<part>: ... (1 of 1)` and `<part>: off for this session`; before, the correction logged one line with the
  exception's code (0 for a C++ exception), the hooks one line without it. A fault in the start is one line
  `start: ...; nothing is hooked` (before, only a C++ exception, without its text). After a fault in the correction
  the game gets its context state back when the plugin's was in its place, the picture and the plugin's views are let
  go, and a third line says so (before, a fault there could leave them held); edworld's `build_info` line then says `correction off`.
- A new `edwide_screen.ini` not written whole is taken away again through its own handle and said in the log with the
  bytes written; "made" is logged only for a file written whole.
- Built with edplugin_common at its commit with the modules `guard` and `settings_doc` (8af49e9).
- The folders, the ini's and the game's settings' reading are edplugin_common's: an `edwide_screen.ini` or the game's
  `Settings.xml` that is there but cannot be read leaves the settings (or the field of view) as they were, with a
  line in the log (before, an unreadable ini was read as an empty one, so the settings went back to their defaults);
  a folder from edloader or a path of the dll over 259 characters is taken whole (before, the dll's folder or a cut
  path).
- The log is edplugin_common's: the last session's `edwide_screen.log` is moved to `edwide_screen\edwide_screen.<UTC>.log` beside it at
  the start (before, each start emptied it), and a line is cut at 2 KB (before, 1 KB).
- Built with edplugin_common 1.0.0, the library the plugins share: a clone in the plugin's directory on the commit
  `edplugin_common.commit` pins. Without the clone the build stops with the commands that make it; another commit or
  changes in it are a warning, and an error in a release build. The log says the library's commit in a line of its
  own after the version (`edplugin_common <hash>`).
- The version in the log: `vX.Y.Z` only for a release; any other build `<the last tag>-<commit's short hash>`, with
  `-dirty` when built from uncommitted changes (before, `git describe`: a build of uncommitted changes on the tag's
  commit said only `vX.Y.Z-dirty`).
- `edwide_screen_build_info` export: the line edworld's `build_info` shows for edwide_screen - its version and the
  wings' state (`wings <angle> deg`, `wings off`, `hook 0`).
- The log through `std::format` (checked when compiled; wide strings as UTF-8) in place of printf's `va_list`; the SRW
  locks held by scope objects.
- The source split by topic (`src/*.cc` with headers, namespace `edwide`) out of one `plugin.cc`; nothing changed in
  what it does.
- The `kdev-*` presets build with clang's GNU-style driver (edplugin_common's `clang-msvc-sdk.cmake`) instead of
  clang-cl, so an IDE whose code model is libclang (KDevelop) reads the compile commands; releases stay with
  clang-cl. Warnings through `edplugin_common_warnings`, linker options with CMake's `LINKER:` prefix.

## 1.0.0-beta.1

- A small element of edloader's chain: the factory hooked in the game's own `D3D11CreateDevice`, after the device is
  made, with no thread of its own (that thread lost the race with the game's swap chain now and then, and EDHM's
  colours went wrong with it). Put it before EDHM in the list. `wings_hook = 0` to tell whether the hook disturbs
  another mod.
- `wings_calibrate` (instead of `wings_guides`): circles, squares and rows across the edges of the middle monitor, to
  set `wings_angle` by; users.md says which way to turn it.
- The hook through the DXGI factory, no device of its own: the first version's probe device and swap chain beside the
  game's ended the game at its start (with EDHM in edloader's list).
- `wings_eye_cm` and `wings_center_cm`: the sides laid as seen from your eyes' distance; the game's field of view is
  used only without them (it put the eyes at ~41 cm for 58 degrees at 9000x2160, so the sides were turned too much
  for eyes 80 cm away).
- First version: the side parts of the picture laid over side monitors turned towards you (angle, curve, zoom,
  offset, bezels, guide lines), the game's field of view read from its Settings.xml, everything in edwide_screen.ini
  read again while the game runs; a picture of another size left alone and said in the log, on again by itself.
