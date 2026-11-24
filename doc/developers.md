# edwide_screen for developers

## How it works

edwide_screen is a plain Windows dll for the game's DirectX 11. It takes the finished picture just before it is shown
(after the game's upscaling and its HUD) and lays each side part over its monitor as the eye sees it through that
monitor. Without it, at a field of view of about 130 degrees across, things near the outer edges are several times
wider than they should be.

The sources in `src/`, namespace `edwide`, each topic a `.cc` with its header of what the others use: `runtime.h`
(the log, the settings, the game's field of view, in `settings_log.cc`), `correction` (the side parts laid over at
each Present; `wings_math.h` its geometry), `hooks` (Present and the DXGI factory), and `entry.cc` (the exports, the
start, `DllMain`).
The log is edplugin_common's `log` module (using-declarations in `runtime.h`): `log_open` at the game's first call
sets the last session's `edwide_screen.log` aside in `edwide_screen\` beside it; `log_line` takes a `std::format` format checked
against its arguments when compiled and hands it to `log_vline` (`std::vformat_to` into 2 KB on the stack, the line
cut there, never thrown); a wide string goes in as
`wide(...)` (UTF-8). SRW locks are held by `exclusive_lock_t` for a scope.
The settings are one table, `settings_table` in `settings_log.cc`: each setting's name, field, group, whether it is
read at the start or while the game runs, its range, its help (the ini's comment) and a note for the doc only.
`make_ini` writes a new ini from it (only when there is none: `CREATE_NEW`, a file not written whole taken away
again), `read_ini` reads by its names, and `write_settings_doc` makes `doc/settings.md` from it through
edplugin_common's `settings_doc`: change the table, then the doc with `tools/doc.sh` (below).

- `src/entry.cc`: a small chain element of edloader's. It exports `D3D11CreateDevice` and
  `D3D11CreateDeviceAndSwapChain` (CMakeLists.txt's `/EXPORT`); `DllMain` does nothing. The game's first call opens
  the log and the ini (edloader's `EDLOADER_LOG_DIR`, `EDLOADER_CONFIG_DIR`; without edloader the dll's folder; once
  in the process, `open_log_and_ini_once`, shared with the export `edwide_screen_write_settings_doc`), then
  is passed on to `d3d11.dll` by name (edloader, which takes it to the next element, or the system's copy). On the
  device made it takes the DXGI factory (`IDXGIDevice` → adapter → parent) and puts hooks into its table of methods:
  `CreateSwapChain` (slot 10) and `CreateSwapChainForHwnd` (slot 15 of `IDXGIFactory2`). The game's factory is of the
  same class (Windows' dxgi, DXVK's), so the swap chain it makes after its device comes through them (one made by
  `D3D11CreateDeviceAndSwapChain` is hooked at once); its table then gets `Present` (slot 8) and `Present1` (slot 22
  of `IDXGISwapChain1`). No thread, no device, no factory and no window of its own: the first version's probe device
  ended the game at its start with EDHM in the list; the second's factory made on a thread of its own lost the race
  with the game's swap chain now and then, and EDHM's colours went wrong with it in the list.
- The log's file of an earlier session: `edloader\logs\edwide_screen\edwide_screen.<UTC of its last write>.log`. Two
  sessions ending in the same second share that name: the second then stays in `edwide_screen.log` and the new one
  goes on after it.
- `wings_hook = 0` (read at the start): the calls only passed on, nothing hooked, to tell whether another mod is
  disturbed by the hook or by the dll being there.
- In the hook, before the original: the ini and the game's `Settings.xml` (`<FOV>`) looked at at most once a second;
  the device from the swap chain, its picture (buffer 0), the layout checked against the picture's size; then the
  whole state of the context set aside (`SwapDeviceContextState` with a state object of its own), the picture copied
  to a texture, one triangle over the picture (`shaders/wings.hlsl`), the state put back. The picture and its render
  target view are taken and let go within the call, so the game's `ResizeBuffers` never meets a reference of
  edwide_screen's; its own copy is made again when the size or format changes. A `Present1` that calls `Present`
  inside is corrected once (a depth per thread).
- Each change of state is one log line: `on`, `on again`, `off: <why>` (wings 0, no layout, another size, no field of
  view, a multisampled picture, objects not made).
- Faults: the plugin's code runs under edplugin_common's guard (`guard/guard.h`), which catches a fault of the
  processor or a C++ exception and logs it (a fault with its place, `<what> at <module>+0x<offset>`; a C++ exception
  with its text); the original calls (the game's `Present`, the next d3d11, the factory's own methods) never run under
  it. Two parts, each off for the session after its first fault, with two log lines `<part>: <the fault> (1 of 1)` and
  `<part>: off for this session (...)`:
  - `correction`, the correction at each Present (`depth` counted outside it). After a fault its handler gives the
    game its context state back when edwide_screen's was in its place (its views unbound first), lets go of what the
    frame held (the picture, its view, the context, the device; each noted in the thread's `frame_t` while held,
    since without `/EHa` a destructor in the frames a fault leaves runs or not; a field is cleared before its
    `Release`, so nothing is let go twice), releases the device's objects, and logs `correction: after the fault ...`.
    The game's `Present` is still called. The handler calls d3d11 again right after a fault that may have been in
    d3d11; a fault in the handler is caught by a guard of its own and logged, and what is left stays held.
  - `hook`, putting the hooks in place: the factory at `D3D11CreateDevice` and Present on each swap chain made
    through the hooked factory. After a fault no swap chain gets Present hooked, and `build_info` says `hook 0`.

  The start (`start()`, once) runs under `run_guarded`: a fault is the line `start: <what> ...; nothing is hooked`.
- The arithmetic (`src/wings_math.h`, the shader the same): the plane the middle monitor's picture stands in is `D`
  pixels from the eye - the eyes' own distance (`wings_eye_cm x wings_center_px / wings_center_cm`) when set, as the
  sides must go on from the middle as the eyes see it whatever the game's field of view; else the distance that field
  of view draws for, `(H/2)/tan(vfov/2)`; a side monitor is turned by the angle about its hinge with the middle; its pixel `a` pixels from
  the hinge is at `P = (half + a cos, y, D - a sin)` (or on a cylinder about the eye, by `wings_curve`); the colour
  comes from where the ray from the eye through `P` meets the picture plane, `(P.x D/P.z, P.y D/P.z)`, then the zoom
  about the hinge and the offset. Behind a bezel, past the eye's side or outside the picture: black. The middle is
  read with `Load`, the same bits.

## Build and test

- `cmake --preset edwide-release` and `cmake --build build/edwide-release -j24` (clang-cl, MSVC_ROOT, fxc under wine;
  the toolchain and the fxc step from edplugin_common, below). The presets `kdev-edwide-debug` and
  `kdev-edwide-release` build without a build directory, for an IDE that chooses its own, with clang's GNU-style
  driver (`edplugin_common/cmake/clang-msvc-sdk.cmake`; clang, clang++, llvm-ar, llvm-ranlib) instead of clang-cl: an
  IDE that hands `compile_commands.json` to libclang (KDevelop) does not take clang-cl's options. Releases are built
  with clang-cl.
- The shared library edplugin_common (the toolchain, CPM with stralgo and small_vectors, the version and fxc build
  steps, the SRW locks and the text helpers such as `wide(...)`) is a clone in `edplugin_common/`, ignored by git, on
  the commit the file `edplugin_common.commit` pins:

  ```
  git clone https://github.com/arturbac/edplugin_common.git edplugin_common
  git -C edplugin_common checkout $(cat edplugin_common.commit)
  ```

  Without it the configure stops with these two commands. At every build the clone is checked against
  `edplugin_common.commit`: another commit, changes in the clone, or the pinned commit missing from it (then `git -C
  edplugin_common fetch origin` first) are a warning, and an error in a release build (HEAD on a tag `vX.Y.Z`, a
  pre-release tag `vX.Y.Z-beta.N` too). The log says the clone's commit in a line of its own at the start
  (`edplugin_common <hash>`, `-dirty` for changes in it, ` (pinned <hash>)` when it is not on the pinned commit). A
  build directory made before the toolchain moved into the clone keeps the old path: configure it again with
  `--fresh`. A commit of the library is pushed to GitHub before a commit of this plugin that pins it. CPM comes with
  the library: `-DCPM_SOURCE_CACHE=<a filled cache> -DFETCHCONTENT_FULLY_DISCONNECTED=ON` configures without the
  network. fxc is found in the SDK by default; `EDPLUGIN_COMMON_FXC` names another one, `EDPLUGIN_COMMON_WINE` another
  wine.
- The version in the log is taken from git at each build (`edplugin_common/cmake/version.cmake`):
  `vX.Y.Z` for a release (HEAD on its tag, no uncommitted changes), otherwise `<the last vX.Y.Z tag>-<HEAD's short
  hash>`, with `-dirty` when tracked files differ from HEAD (`v0.0.0-<hash>` before the first tag).
- `tools/doc.sh <build dir> <scratch>` (`EDWIDE_WINEPREFIX`): `test/doc_app.cc` under wine calls the export
  `edwide_screen_write_settings_doc` (the log and the ini as at the game's start, no device, no drawing; with
  `EDWIDE_SETTINGS_DOC` set, the doc too; it does nothing once the game's start has begun). Checked: a fresh ini is
  `test/data/fresh.ini`, an ini already there is left as it was, `read_ini`'s log lines for `test/data/kept.ini` and
  `test/data/bad.ini` are `kept_log.txt` and `bad_log.txt`, `doc/settings.md` is up to date (it prints the `cp` that
  brings it up to date), and each of the ini's 13 settings is a row of the doc with the same default.
- `tools/test.sh <build dir> <scratch>` (`EDWIDE_WINEPREFIX`): `test/test_app.cc` loads the plugin as edloader would,
  presents a picture 960x240 laid out as 480 + 2 x 240 with known colours and reads it back: the middle the same bits,
  each side's pixel the colour of where `wings_math.h` says it comes from (within 3 of 255), angle 0 and wings 0 change
  nothing, `ResizeBuffers` to another size is not refused and that size passes untouched with its log line, the right
  size turns it on again, `wings_calibrate = 0` leaves no trace of the shapes. Leaves `grid.bmp`, `grid_30.bmp` and
  `calibrate_20/30/40.bmp` to look at. Then a run with `wings_hook = 0` and, last, `tools/doc.sh`. No test makes a
  fault: the guard's paths are checked by edplugin_common's own test.
- The calibration (`calibration()` in the shader) is drawn in the picture's own pixels, before the sides are laid:
  the shapes stand on the middle monitor's plane carried on, so they are right on the monitors exactly when the
  angle and the eyes' distance are; the line is a pixel and a half of the monitor wide wherever it is.
- `tools/bench.sh <build dir> <scratch> [w h]` (`EDWIDE_WINEPREFIX`, `EDWIDE_DXVK_DIR` with DXVK's d3d11.dll and
  dxgi.dll): a frame's time on the graphics card with the correction less without it, median of 100.
- With no `DISPLAY` (Wayland only) both scripts set `WINE_USE_EGL=1` unless it is set already: wine's OpenGL, which
  wined3d needs, comes then only through EGL.
