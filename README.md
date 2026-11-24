# edwide_screen

A plugin for [edloader](https://github.com/arturbac/edloader): Elite Dangerous on **three monitors with the side
ones turned towards the player**, without stretched edges.

![The calibration's shapes on three monitors](doc/images/calibration.jpg)

- **What it does:** the middle monitor stays as it was; the sides are squeezed back to the right shape; straight
  lines stay straight across the monitors' edges.
- **Needs:** edloader. Works with or without edworld.
- **Tried on:** Linux, Proton (DXVK), three 4K monitors. Windows: not tried yet.

**Install and set up:** [doc/users.md](doc/users.md) · **All settings:** [doc/settings.md](doc/settings.md)

Build and test (Linux, clang-cl and wine): edplugin_common cloned into `edplugin_common/` on the commit
`edplugin_common.commit` pins, `MSVC_ROOT` naming an MSVC SDK as msvc-wine lays it out, the first configure
fetching its libraries through CPM (or `-DCPM_SOURCE_CACHE=<a filled cache> -DFETCHCONTENT_FULLY_DISCONNECTED=ON`);
then `cmake --preset edwide-release`, `cmake --build build/edwide-release` and
`EDWIDE_WINEPREFIX=<a wine prefix> tools/test.sh build/edwide-release <scratch dir>` (the script's head says the rest).

License: MIT ([LICENSE](LICENSE))
