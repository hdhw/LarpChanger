#pragma once

#include <windows.h>

#include <mmsystem.h>
#include <tlhelp32.h>

#include <string>

namespace larp {

inline std::wstring ownFolder() {
  wchar_t Self[MAX_PATH];
  const DWORD Length = GetModuleFileNameW(nullptr, Self, MAX_PATH);
  std::wstring Folder(Self, Length);
  Folder.resize(Folder.find_last_of(L'\\'));
  return Folder;
}

inline DWORD findCs2() {
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

enum class Result { Loaded, Refused, Failed };

inline Result inject(DWORD Pid, const std::wstring &Dll) {
  HANDLE Process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                   PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                               FALSE, Pid);
  if (Process == nullptr) {
    return Result::Failed;
  }
  const SIZE_T Bytes = (Dll.size() + 1) * sizeof(wchar_t);
  void *const Remote =
      VirtualAllocEx(Process, nullptr, Bytes, MEM_COMMIT, PAGE_READWRITE);
  if (Remote == nullptr ||
      !WriteProcessMemory(Process, Remote, Dll.c_str(), Bytes, nullptr)) {
    CloseHandle(Process);
    return Result::Failed;
  }
  auto *const Load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  HANDLE Thread =
      CreateRemoteThread(Process, nullptr, 0, Load, Remote, 0, nullptr);
  if (Thread == nullptr) {
    VirtualFreeEx(Process, Remote, 0, MEM_RELEASE);
    CloseHandle(Process);
    return Result::Failed;
  }
  WaitForSingleObject(Thread, INFINITE);
  DWORD Module = 0;
  GetExitCodeThread(Thread, &Module);
  CloseHandle(Thread);
  VirtualFreeEx(Process, Remote, 0, MEM_RELEASE);
  CloseHandle(Process);
  return Module == 0 ? Result::Refused : Result::Loaded;
}

inline std::wstring settingsFolder() {
  const wchar_t *const AppData = _wgetenv(L"APPDATA");
  return AppData == nullptr ? std::wstring()
                            : std::wstring(AppData) + L"\\openchanger";
}

// First load plays Welcome.mp3, later loads WelcomeBack.mp3; a marker file remembers which.
inline void playWelcome(const std::wstring &Sounds) {
  const std::wstring Settings = settingsFolder();
  if (Settings.empty()) {
    return;
  }
  CreateDirectoryW(Settings.c_str(), nullptr);
  const std::wstring Marker = Settings + L"\\welcomed";
  const bool Back =
      GetFileAttributesW(Marker.c_str()) != INVALID_FILE_ATTRIBUTES;
  const std::wstring Sound =
      Sounds + (Back ? L"\\WelcomeBack.mp3" : L"\\Welcome.mp3");
  const std::wstring Open =
      L"open \"" + Sound + L"\" type mpegvideo alias larpwelcome";
  if (mciSendStringW(Open.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW(L"play larpwelcome wait", nullptr, 0, nullptr);
    mciSendStringW(L"close larpwelcome", nullptr, 0, nullptr);
  }
  HANDLE File = CreateFileW(Marker.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (File != INVALID_HANDLE_VALUE) {
    CloseHandle(File);
  }
}

} // namespace larp
