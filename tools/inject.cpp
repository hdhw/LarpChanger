#include <windows.h>

#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>

namespace {

DWORD findCs2() {
  HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (Snapshot == INVALID_HANDLE_VALUE) {
    return 0;
  }
  PROCESSENTRY32W Entry{.dwSize = sizeof(Entry)};
  DWORD Pid = 0;
  for (BOOL More = Process32FirstW(Snapshot, &Entry); More && Pid == 0;
       More = Process32NextW(Snapshot, &Entry)) {
    if (_wcsicmp(Entry.szExeFile, L"cs2.exe") == 0) {
      Pid = Entry.th32ProcessID;
    }
  }
  CloseHandle(Snapshot);
  return Pid;
}

int fail(const char *What) {
  std::printf("%s (error %lu)\n", What, GetLastError());
  return 1;
}

} // namespace

int main() {
  wchar_t Dll[MAX_PATH];
  const DWORD Length = GetModuleFileNameW(nullptr, Dll, MAX_PATH);
  wchar_t *const Slash = std::wcsrchr(Dll, L'\\');
  if (Length == 0 || Length == MAX_PATH || Slash == nullptr) {
    return fail("cannot resolve own path");
  }
  std::wcscpy(Slash + 1, L"larpchanger.dll");
  if (GetFileAttributesW(Dll) == INVALID_FILE_ATTRIBUTES) {
    return fail("larpchanger.dll is not next to larpchanger_inject.exe");
  }

  const DWORD Pid = findCs2();
  if (Pid == 0) {
    std::printf("cs2.exe is not running. Launch it with -insecure first.\n");
    return 1;
  }
  HANDLE Process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                   PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                               FALSE, Pid);
  if (Process == nullptr) {
    return fail("cannot open cs2.exe");
  }
  const SIZE_T Bytes = (std::wcslen(Dll) + 1) * sizeof(wchar_t);
  void *const Remote =
      VirtualAllocEx(Process, nullptr, Bytes, MEM_COMMIT, PAGE_READWRITE);
  if (Remote == nullptr || !WriteProcessMemory(Process, Remote, Dll, Bytes, nullptr)) {
    return fail("cannot write into cs2.exe");
  }
  auto *const Load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  HANDLE Thread = CreateRemoteThread(Process, nullptr, 0, Load, Remote, 0, nullptr);
  if (Thread == nullptr) {
    return fail("cannot start the loader thread");
  }
  WaitForSingleObject(Thread, INFINITE);
  DWORD Module = 0;
  GetExitCodeThread(Thread, &Module);
  CloseHandle(Thread);
  VirtualFreeEx(Process, Remote, 0, MEM_RELEASE);
  CloseHandle(Process);

  if (Module == 0) {
    std::printf("larpchanger.dll refused to load: cs2.exe was not started with "
                "-insecure.\n");
    return 1;
  }
  std::printf("loaded into cs2.exe (pid %lu)\n", Pid);
  return 0;
}