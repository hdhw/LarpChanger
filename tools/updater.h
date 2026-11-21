#pragma once

#include <windows.h>

#include <winhttp.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>

namespace larp {

constexpr const wchar_t *ReleasesApi =
    L"https://api.github.com/repos/hdhw/LarpChanger/releases/latest";
// Only binaries from this repo's own releases are ever installed.
constexpr const char *TrustedDownload =
    "https://github.com/hdhw/LarpChanger/releases/download/";

inline bool httpsGet(const std::wstring &Url,
                     const std::function<bool(const char *, DWORD, DWORD)> &Chunk) {
  wchar_t Host[256];
  wchar_t Path[2048];
  wchar_t Extra[1024];
  URL_COMPONENTS Parts{};
  Parts.dwStructSize = sizeof(Parts);
  Parts.lpszHostName = Host;
  Parts.dwHostNameLength = 256;
  Parts.lpszUrlPath = Path;
  Parts.dwUrlPathLength = 2048;
  Parts.lpszExtraInfo = Extra;
  Parts.dwExtraInfoLength = 1024;
  if (!WinHttpCrackUrl(Url.c_str(), 0, 0, &Parts) ||
      Parts.nScheme != INTERNET_SCHEME_HTTPS) {
    return false;
  }
  const std::wstring Target = std::wstring(Path) + Extra;
  HINTERNET Session = WinHttpOpen(L"LarpChanger-Updater",
                                  WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (Session == nullptr) {
    return false;
  }
  WinHttpSetTimeouts(Session, 5000, 5000, 5000, 15000);
  HINTERNET Connection = WinHttpConnect(Session, Host, Parts.nPort, 0);
  HINTERNET Request =
      Connection == nullptr
          ? nullptr
          : WinHttpOpenRequest(Connection, L"GET", Target.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               WINHTTP_FLAG_SECURE);
  bool Ok = Request != nullptr &&
            WinHttpSendRequest(Request, L"Accept: application/vnd.github+json\r\n",
                               static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0,
                               0, 0) &&
            WinHttpReceiveResponse(Request, nullptr);
  DWORD Status = 0;
  DWORD Total = 0;
  DWORD Size = sizeof(DWORD);
  if (Ok) {
    WinHttpQueryHeaders(Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &Status, &Size,
                        WINHTTP_NO_HEADER_INDEX);
    Size = sizeof(DWORD);
    WinHttpQueryHeaders(Request,
                        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &Total, &Size,
                        WINHTTP_NO_HEADER_INDEX);
    Ok = Status == 200;
  }
  char Buffer[16384];
  for (DWORD Read = 0; Ok;) {
    Ok = WinHttpReadData(Request, Buffer, sizeof(Buffer), &Read) == TRUE;
    if (!Ok || Read == 0) {
      break;
    }
    Ok = Chunk(Buffer, Read, Total);
  }
  if (Request != nullptr) {
    WinHttpCloseHandle(Request);
  }
  if (Connection != nullptr) {
    WinHttpCloseHandle(Connection);
  }
  WinHttpCloseHandle(Session);
  return Ok;
}

using Version = std::array<int, 3>;

inline Version parseVersion(const std::string &Text) {
  Version Parsed{};
  const char *const Start = Text.c_str() + (!Text.empty() && Text[0] == 'v');
  std::sscanf(Start, "%d.%d.%d", &Parsed[0], &Parsed[1], &Parsed[2]);
  return Parsed;
}

inline std::string jsonString(const std::string &Json, const std::string &Key,
                              std::size_t From = 0, std::size_t *End = nullptr) {
  const std::string Needle = "\"" + Key + "\":\"";
  const std::size_t Start = Json.find(Needle, From);
  if (Start == std::string::npos) {
    return {};
  }
  const std::size_t Value = Start + Needle.size();
  const std::size_t Close = Json.find('"', Value);
  if (End != nullptr) {
    *End = Close;
  }
  return Close == std::string::npos ? std::string() : Json.substr(Value, Close - Value);
}

class Updater {
public:
  enum class State { Checking, UpToDate, Downloading, Ready, Failed };

  std::atomic<State> Now{State::Checking};
  std::atomic<float> Progress{0.0f};

  std::string latest() {
    const std::lock_guard Lock(Mutex);
    return Latest;
  }

  std::wstring downloaded() {
    const std::lock_guard Lock(Mutex);
    return Downloaded;
  }

  void run(const std::string &Current, const std::wstring &Self) {
    std::string Json;
    const bool Fetched = httpsGet(ReleasesApi, [&](const char *Data, DWORD Length, DWORD) {
      Json.append(Data, Length);
      return Json.size() < 1'000'000;
    });
    const std::string Tag = jsonString(Json, "tag_name");
    if (!Fetched || Tag.empty()) {
      Now = State::Failed;
      return;
    }
    {
      const std::lock_guard Lock(Mutex);
      Latest = Tag;
    }
    if (parseVersion(Tag) <= parseVersion(Current)) {
      Now = State::UpToDate;
      return;
    }
    std::string Asset;
    for (std::size_t Cursor = 0;;) {
      const std::string Url = jsonString(Json, "browser_download_url", Cursor, &Cursor);
      if (Url.empty()) {
        break;
      }
      if (Url.starts_with(TrustedDownload) && Url.ends_with("/LarpChanger.exe")) {
        Asset = Url;
        break;
      }
    }
    if (Asset.empty()) {
      Now = State::Failed;
      return;
    }
    Now = State::Downloading;
    const std::wstring Temporary = Self + L".new";
    HANDLE File = CreateFileW(Temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (File == INVALID_HANDLE_VALUE) {
      Now = State::Failed;
      return;
    }
    DWORD Received = 0;
    char Header[2] = {};
    const bool Ok = httpsGet(std::wstring(Asset.begin(), Asset.end()),
                             [&](const char *Data, DWORD Length, DWORD Total) {
                               if (Received == 0 && Length >= 2) {
                                 Header[0] = Data[0];
                                 Header[1] = Data[1];
                               }
                               Received += Length;
                               if (Total != 0) {
                                 Progress = static_cast<float>(Received) /
                                            static_cast<float>(Total);
                               }
                               DWORD Written = 0;
                               return WriteFile(File, Data, Length, &Written, nullptr) &&
                                      Written == Length;
                             });
    CloseHandle(File);
    if (!Ok || Header[0] != 'M' || Header[1] != 'Z' || Received < 1'000'000) {
      DeleteFileW(Temporary.c_str());
      Now = State::Failed;
      return;
    }
    {
      const std::lock_guard Lock(Mutex);
      Downloaded = Temporary;
    }
    Progress = 1.0f;
    Now = State::Ready;
  }

private:
  std::mutex Mutex;
  std::string Latest;
  std::wstring Downloaded;
};

// Swaps the running exe for the downloaded one and starts it. The running exe can be renamed, not overwritten.
inline bool applyUpdate(const std::wstring &Self, const std::wstring &Downloaded) {
  const std::wstring Old = Self + L".old";
  DeleteFileW(Old.c_str());
  if (!MoveFileExW(Self.c_str(), Old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    return false;
  }
  if (!MoveFileExW(Downloaded.c_str(), Self.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    MoveFileExW(Old.c_str(), Self.c_str(), MOVEFILE_REPLACE_EXISTING);
    return false;
  }
  STARTUPINFOW Startup{};
  Startup.cb = sizeof(Startup);
  PROCESS_INFORMATION Process{};
  if (!CreateProcessW(Self.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &Startup, &Process)) {
    return false;
  }
  CloseHandle(Process.hThread);
  CloseHandle(Process.hProcess);
  return true;
}

} // namespace larp
