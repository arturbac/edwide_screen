# Changelog

## Unreleased

- `edwide_screen_build_info` export: the line edworld's `build_info` shows for edwide_screen - its version and the
  wings' state (`wings <angle> deg`, `wings off`, `hook 0`).
- The log through `std::format` (checked when compiled; wide strings as UTF-8) in place of printf's `va_list`; the SRW
  locks held by scope objects.
- The source split by topic (`src/*.cc` with headers, namespace `edwide`) out of one `plugin.cc`; nothing changed in
  what it does.

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
