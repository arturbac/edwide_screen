// tools/doc.sh's program: edwide_screen.dll loaded beside it and its export edwide_screen_write_settings_doc called,
// which makes the log and the ini as at the game's start with no device and no drawing. Exit code 0 when the export
// said done.
#include <windows.h>

#include <cstdio>

auto main() -> int
  {
  HMODULE const plugin{LoadLibraryW(L"edwide_screen.dll")};
  if(not plugin)
    {
    std::printf("FAIL: edwide_screen.dll not loaded (error %lu)\n", GetLastError());
    return 1;
    }
  using write_t = int(WINAPI *)();
  auto const write{reinterpret_cast<write_t>(reinterpret_cast<void *>(GetProcAddress(plugin, "edwide_screen_write_settings_doc")))};
  if(not write)
    {
    std::printf("FAIL: no export edwide_screen_write_settings_doc\n");
    return 1;
    }
  int const rc{write()};
  std::printf(rc == 0 ? "ok  : edwide_screen_write_settings_doc\n" : "FAIL: edwide_screen_write_settings_doc said %d\n", rc);
  return rc;
  }
