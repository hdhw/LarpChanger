#include "loader.h"

#include <cstdio>
#include <string>

int main() {
  const std::wstring Folder = larp::ownFolder();
  const std::wstring Dll = Folder + L"\\larpchanger.dll";
  if (GetFileAttributesW(Dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::printf("larpchanger.dll is not next to larpchanger_inject.exe\n");
    return 1;
  }
  const DWORD Pid = larp::findCs2();
  if (Pid == 0) {
    std::printf("cs2.exe is not running. Launch it with -insecure first.\n");
    return 1;
  }
  switch (larp::inject(Pid, Dll)) {
  case larp::Result::Loaded:
    std::printf("loaded into cs2.exe (pid %lu)\n", Pid);
    larp::playWelcome(Folder + L"\\sounds");
    return 0;
  case larp::Result::Refused:
    std::printf("larpchanger.dll refused to load: cs2.exe was not started "
                "with -insecure.\n");
    return 1;
  default:
    std::printf("could not load into cs2.exe (error %lu)\n", GetLastError());
    return 1;
  }
}
