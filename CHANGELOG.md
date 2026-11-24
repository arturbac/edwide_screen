# Changelog

## Unreleased

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
