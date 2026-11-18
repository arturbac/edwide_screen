# edwide_screen for developers

## How it works

- `src/plugin.cc`: `DllMain` only starts a thread. The thread opens the log and the ini (edloader's
  `EDLOADER_LOG_DIR`, `EDLOADER_CONFIG_DIR`; without edloader the dll's folder), then makes a swap chain of the
  system's own d3d11 (by its path, not `d3d11.dll` by name, which is edloader: its chain would take the device for the
  game's) on a hidden window, and puts its hooks into that swap chain's table of methods: `Present` (slot 8) and
  `Present1` (slot 22 of `IDXGISwapChain1`). The game's swap chain is of the same class (Windows' dxgi, DXVK's), so
  it comes through the hooks. `EDWIDE_PROBE_BY_NAME` (the bench only) takes `d3d11.dll` by name.
- In the hook, before the original: the ini and the game's `Settings.xml` (`<FOV>`) looked at at most once a second;
  the device from the swap chain, its picture (buffer 0), the layout checked against the picture's size; then the
  whole state of the context set aside (`SwapDeviceContextState` with a state object of its own), the picture copied
  to a texture, one triangle over the picture (`shaders/wings.hlsl`), the state put back. The picture and its render
  target view are taken and let go within the call, so the game's `ResizeBuffers` never meets a reference of
  edwide_screen's; its own copy is made again when the size or format changes. A `Present1` that calls `Present`
  inside is corrected once (a depth per thread).
- Each change of state is one log line: `on`, `on again`, `off: <why>` (wings 0, no layout, another size, no field of
  view, a multisampled picture, objects not made). A fault or C++ exception in the hook turns it off for the session,
  logged; the game's `Present` is still called.
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
  `cmake/` as edworld's).
- `tools/test.sh <build dir> <scratch>` (`EDWIDE_WINEPREFIX`): `test/test_app.cc` loads the plugin as edloader would,
  presents a picture 960x240 laid out as 480 + 2 x 240 with known colours and reads it back: the middle the same bits,
  each side's pixel the colour of where `wings_math.h` says it comes from (within 3 of 255), angle 0 and wings 0 change
  nothing, `ResizeBuffers` to another size is not refused and that size passes untouched with its log line, the right
  size turns it on again. Leaves `grid.bmp`, `grid_30.bmp`, `grid_30_guides.bmp` to look at.
- `tools/bench.sh <build dir> <scratch> [w h]` (`EDWIDE_WINEPREFIX`, `EDWIDE_DXVK_DIR` with DXVK's d3d11.dll and
  dxgi.dll): a frame's time on the graphics card with the correction less without it, median of 100.
