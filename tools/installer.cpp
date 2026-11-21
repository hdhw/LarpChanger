#include "loader.h"
#include "updater.h"

#include <d3d11.h>
#include <dwmapi.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
                                                             LPARAM);

namespace {

constexpr int Width = 440;
constexpr int Height = 380;
constexpr int DllResource = 101;
constexpr int WelcomeResource = 102;
constexpr int WelcomeBackResource = 103;
constexpr float Pi = 3.14159265f;
constexpr ImVec2 StarCentre{Width * 0.5f, 92.0f};
constexpr float StarRadius = 36.0f;

ID3D11Device *Device = nullptr;
ID3D11DeviceContext *Context = nullptr;
IDXGISwapChain *SwapChain = nullptr;
ID3D11RenderTargetView *Target = nullptr;

// Palette: near-black base, one blue accent for the star, green and red only for success and failure.
const ImVec4 Base{0.051f, 0.059f, 0.075f, 1.0f};
const ImVec4 Star{0.231f, 0.510f, 0.965f, 1.0f};
const ImVec4 ButtonBlue{0.145f, 0.388f, 0.922f, 1.0f};
const ImVec4 Ink{0.94f, 0.95f, 0.97f, 1.0f};
const ImVec4 Muted{0.60f, 0.63f, 0.69f, 1.0f};
const ImVec4 Good{0.35f, 0.84f, 0.55f, 1.0f};
const ImVec4 Bad{0.97f, 0.44f, 0.44f, 1.0f};

enum class Load { Idle, Loading, Loaded, Refused, Failed };
enum class Phase { Intro, Checking, Downloading, Installing, Ready, Loading, Loaded, Refused, Failed };

std::atomic<Load> LoadState{Load::Idle};
std::atomic<DWORD> GamePid{0};
larp::Updater Update;
std::wstring Folder;
std::wstring Self;
bool Installed = false;
bool UpdateFailed = false;

struct Fonts {
  ImFont *Body = nullptr;
  ImFont *Title = nullptr;
  ImFont *Button = nullptr;
};

float clamp01(float Value) { return std::clamp(Value, 0.0f, 1.0f); }
float easeOutCubic(float T) { return 1.0f - std::pow(1.0f - clamp01(T), 3.0f); }
float easeOutBack(float T) {
  const float C = 1.70158f;
  const float X = clamp01(T) - 1.0f;
  return 1.0f + (C + 1.0f) * X * X * X + C * X * X;
}
ImVec4 mix(ImVec4 A, ImVec4 B, float T) {
  return {A.x + (B.x - A.x) * T, A.y + (B.y - A.y) * T, A.z + (B.z - A.z) * T,
          A.w + (B.w - A.w) * T};
}
ImU32 colour(ImVec4 C, float Alpha = 1.0f) {
  return ImGui::GetColorU32({C.x, C.y, C.z, C.w * Alpha});
}

bool writeResource(int Id, const std::wstring &Path) {
  HRSRC Info = FindResourceW(nullptr, MAKEINTRESOURCEW(Id), RT_RCDATA);
  HGLOBAL Data = Info == nullptr ? nullptr : LoadResource(nullptr, Info);
  if (Data == nullptr) {
    return false;
  }
  HANDLE File = CreateFileW(Path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (File == INVALID_HANDLE_VALUE) {
    return false;
  }
  DWORD Written = 0;
  const DWORD Size = SizeofResource(nullptr, Info);
  const BOOL Ok = WriteFile(File, LockResource(Data), Size, &Written, nullptr);
  CloseHandle(File);
  return Ok && Written == Size;
}

// Unpacks the DLL and sounds into %LOCALAPPDATA%\LarpChanger. A DLL locked by a running CS2 is kept as is.
bool install() {
  const wchar_t *const Local = _wgetenv(L"LOCALAPPDATA");
  if (Local == nullptr) {
    return false;
  }
  Folder = std::wstring(Local) + L"\\LarpChanger";
  CreateDirectoryW(Folder.c_str(), nullptr);
  CreateDirectoryW((Folder + L"\\sounds").c_str(), nullptr);
  const std::wstring Dll = Folder + L"\\larpchanger.dll";
  const bool DllOk = writeResource(DllResource, Dll) ||
                     GetFileAttributesW(Dll.c_str()) != INVALID_FILE_ATTRIBUTES;
  writeResource(WelcomeResource, Folder + L"\\sounds\\Welcome.mp3");
  writeResource(WelcomeBackResource, Folder + L"\\sounds\\WelcomeBack.mp3");
  return DllOk;
}

void startLoading(DWORD Pid) {
  LoadState = Load::Loading;
  std::thread([Pid] {
    const larp::Result Result = larp::inject(Pid, Folder + L"\\larpchanger.dll");
    LoadState = Result == larp::Result::Loaded    ? Load::Loaded
                : Result == larp::Result::Refused ? Load::Refused
                                                  : Load::Failed;
    if (Result == larp::Result::Loaded) {
      larp::playWelcome(Folder + L"\\sounds");
    }
  }).detach();
}

void startBackground() {
  std::thread([] { Update.run(LARP_VERSION, Self); }).detach();
  std::thread([] {
    Sleep(2000);
    DeleteFileW((Self + L".old").c_str());
    for (;;) {
      const DWORD Pid = larp::findCs2();
      if (Pid != GamePid.exchange(Pid) && LoadState != Load::Loading) {
        LoadState = Load::Idle;
      }
      Sleep(1000);
    }
  }).detach();
}

Phase currentPhase(double Time) {
  const larp::Updater::State Check = Update.Now;
  if (Time < 1.2) {
    return Phase::Intro;
  }
  if (Check == larp::Updater::State::Checking) {
    return Phase::Checking;
  }
  if (Check == larp::Updater::State::Downloading) {
    return Phase::Downloading;
  }
  if (Check == larp::Updater::State::Ready && !UpdateFailed) {
    return Phase::Installing;
  }
  switch (LoadState.load()) {
  case Load::Loading:
    return Phase::Loading;
  case Load::Loaded:
    return Phase::Loaded;
  case Load::Refused:
    return Phase::Refused;
  case Load::Failed:
    return Phase::Failed;
  default:
    return Phase::Ready;
  }
}

std::string statusText(Phase Now) {
  char Text[160];
  switch (Now) {
  case Phase::Intro:
  case Phase::Checking:
    return "Checking for updates";
  case Phase::Downloading:
    std::snprintf(Text, sizeof(Text), "Downloading %s  %d%%", Update.latest().c_str(),
                  static_cast<int>(Update.Progress * 100.0f));
    return Text;
  case Phase::Installing:
    return "Installing update";
  case Phase::Loading:
    return "Loading into CS2";
  case Phase::Loaded:
    return "Loaded. Open the star tab in CS2's main menu.";
  case Phase::Refused:
    return "CS2 wasn't started with -insecure. Restart it with -insecure.";
  case Phase::Failed:
    return "Couldn't load into CS2. Try again, or run as administrator.";
  default:
    if (!Installed) {
      return "Couldn't unpack LarpChanger into %LOCALAPPDATA%.";
    }
    return GamePid == 0 ? "Start CS2 with -insecure, then load it here."
                        : "CS2 is running. Ready when you are.";
  }
}

ImVec4 statusColour(Phase Now) {
  switch (Now) {
  case Phase::Loaded:
    return Good;
  case Phase::Refused:
  case Phase::Failed:
    return Bad;
  default:
    return !Installed && Now == Phase::Ready ? Bad : Muted;
  }
}

// Traces one triangle's outline from its first corner; Amount runs 0..1 around the perimeter.
void traceTriangle(ImDrawList *List, const ImVec2 (&Points)[3], float Amount, ImU32 Colour,
                   float Thickness) {
  const float Length = clamp01(Amount) * 3.0f;
  for (int Edge = 0; Edge < 3 && Length > Edge; ++Edge) {
    const ImVec2 From = Points[Edge];
    const ImVec2 To = Points[(Edge + 1) % 3];
    const float Part = std::min(1.0f, Length - static_cast<float>(Edge));
    const ImVec2 End{From.x + (To.x - From.x) * Part, From.y + (To.y - From.y) * Part};
    List->AddLine(From, End, Colour, Thickness);
    List->AddCircleFilled(From, Thickness * 0.5f, Colour, 12);
    List->AddCircleFilled(End, Thickness * 0.5f, Colour, 12);
  }
}

void drawStar(ImDrawList *List, ImVec2 Centre, float Radius, float Drawn, ImVec4 Tint) {
  for (int Each = 0; Each < 2; ++Each) {
    const float Offset = Each == 0 ? -90.0f : 90.0f;
    ImVec2 Points[3];
    for (int Corner = 0; Corner < 3; ++Corner) {
      const float Angle = (Offset + 120.0f * static_cast<float>(Corner)) * Pi / 180.0f;
      Points[Corner] = {Centre.x + Radius * std::cos(Angle), Centre.y + Radius * std::sin(Angle)};
    }
    const float Delay = Each * 0.25f;
    traceTriangle(List, Points, (Drawn - Delay) / (1.0f - 0.25f), colour(Tint), 4.0f);
  }
}

void drawGlow(ImDrawList *List, ImVec2 Centre, float Strength, ImVec4 Tint) {
  for (int Ring = 24; Ring >= 1; --Ring) {
    const float Radius = 28.0f + Ring * 2.6f;
    List->AddCircleFilled(Centre, Radius, colour(Tint, 0.011f * Strength), 64);
  }
}

void drawArc(ImDrawList *List, ImVec2 Centre, float Radius, float From, float To, ImVec4 Tint,
             float Alpha, float Thickness) {
  List->PathClear();
  List->PathArcTo(Centre, Radius, From, To, 48);
  List->PathStroke(colour(Tint, Alpha), ImDrawFlags_None, Thickness);
}

// The emblem carries every state: drawn in on launch, a ring while working, a burst on success, a shake on failure.
void drawEmblem(Phase Now, double Time, double PhaseTime) {
  ImDrawList *const List = ImGui::GetWindowDrawList();
  const float Since = static_cast<float>(PhaseTime);
  ImVec2 Centre = StarCentre;
  ImVec4 Tint = Star;
  if (Now == Phase::Refused || Now == Phase::Failed) {
    const float Shake = clamp01(Since / 0.5f);
    Centre.x += std::sin(Since * 48.0f) * 7.0f * (1.0f - Shake);
    Tint = mix(Bad, Star, easeOutCubic(Since / 1.2f));
  }
  const float Intro = static_cast<float>(Time) / 1.1f;
  const float Bloom = easeOutCubic((static_cast<float>(Time) - 0.6f) / 0.8f);
  const bool Working = Now == Phase::Checking || Now == Phase::Loading || Now == Phase::Installing;
  drawGlow(List, Centre, Bloom * (Working ? 1.35f : 1.0f), Tint);
  drawArc(List, Centre, 54.0f, 0.0f, 2.0f * Pi, Ink, 0.06f * Bloom, 2.0f);
  if (Working) {
    const float Spin = static_cast<float>(Time) * 2.0f * Pi * 0.9f;
    drawArc(List, Centre, 54.0f, Spin, Spin + Pi * 0.55f, Star, clamp01(Since / 0.25f), 3.0f);
  } else if (Now == Phase::Downloading) {
    drawArc(List, Centre, 54.0f, -Pi * 0.5f, -Pi * 0.5f + 2.0f * Pi * Update.Progress, Star, 1.0f,
            3.0f);
  } else if (Now == Phase::Loaded && Since < 0.9f) {
    const float Burst = easeOutCubic(Since / 0.9f);
    drawArc(List, Centre, 54.0f + Burst * 90.0f, 0.0f, 2.0f * Pi, Good, 1.0f - Burst,
            3.0f * (1.0f - Burst) + 0.5f);
  }
  drawStar(List, Centre, StarRadius * (0.92f + 0.08f * easeOutBack(Intro)), easeOutCubic(Intro),
           Tint);
}

// Each letter rises 8px and fades in, staggered, so the name arrives after the star starts drawing.
void revealTitle(const Fonts &Font, double Time) {
  const char *const Name = "LarpChanger";
  ImGui::PushFont(Font.Title);
  const ImVec2 Size = ImGui::CalcTextSize(Name);
  const ImVec2 Window = ImGui::GetWindowPos();
  const float Left = Window.x + (Width - Size.x) * 0.5f;
  ImDrawList *const List = ImGui::GetWindowDrawList();
  for (int Index = 0; Name[Index] != '\0'; ++Index) {
    const float T = easeOutCubic((static_cast<float>(Time) - 0.45f - Index * 0.035f) / 0.45f);
    const float X = Left + ImGui::CalcTextSize(Name, Name + Index).x;
    const char Letter[2] = {Name[Index], '\0'};
    List->AddText({X, Window.y + 150.0f + (1.0f - T) * 8.0f}, colour(Ink, T), Letter);
  }
  ImGui::PopFont();
}

// Status changes slide: the old line lifts and fades out while the new one rises in.
void statusLine(const std::string &Text, ImVec4 Tint, double Time) {
  static std::string Shown;
  static std::string Previous;
  static ImVec4 PreviousTint;
  static ImVec4 ShownTint;
  static double Changed = -10.0;
  if (Text != Shown) {
    Previous = Shown;
    PreviousTint = ShownTint;
    Shown = Text;
    Changed = Time;
  }
  ShownTint = Tint;
  const float T = easeOutCubic(static_cast<float>(Time - Changed) / 0.35f);
  ImDrawList *const List = ImGui::GetWindowDrawList();
  const ImVec2 Window = ImGui::GetWindowPos();
  const float Y = Window.y + 234.0f;
  if (T < 1.0f && !Previous.empty()) {
    const float Out = ImGui::CalcTextSize(Previous.c_str()).x;
    List->AddText({Window.x + (Width - Out) * 0.5f, Y - T * 10.0f},
                  colour(PreviousTint, 1.0f - T), Previous.c_str());
  }
  const float In = ImGui::CalcTextSize(Shown.c_str()).x;
  List->AddText({Window.x + (Width - In) * 0.5f, Y + (1.0f - T) * 10.0f}, colour(Tint, T),
                Shown.c_str());
}

void pointerOnHover(bool Disabled = false) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetMouseCursor(Disabled ? ImGuiMouseCursor_NotAllowed : ImGuiMouseCursor_Hand);
  }
}

bool closeButton() {
  ImGui::SetCursorPos({Width - 46.0f, 12.0f});
  const bool Pressed = ImGui::InvisibleButton("Close", {34.0f, 34.0f});
  const ImVec2 Min = ImGui::GetItemRectMin();
  const ImVec2 Max = ImGui::GetItemRectMax();
  ImDrawList *const List = ImGui::GetWindowDrawList();
  const bool Hovered = ImGui::IsItemHovered();
  if (Hovered || ImGui::IsItemActive()) {
    List->AddRectFilled(Min, Max, colour(Ink, ImGui::IsItemActive() ? 0.14f : 0.08f), 8.0f);
  }
  const ImVec2 C{(Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f};
  const ImU32 Mark = colour(Hovered ? Ink : Muted);
  List->AddLine({C.x - 5, C.y - 5}, {C.x + 5, C.y + 5}, Mark, 1.6f);
  List->AddLine({C.x + 5, C.y - 5}, {C.x - 5, C.y + 5}, Mark, 1.6f);
  pointerOnHover();
  return Pressed;
}

void actionButton(const Fonts &Font, Phase Now, double Time) {
  const DWORD Pid = GamePid;
  const bool CanLoad = Installed && Pid != 0 &&
                       (Now == Phase::Ready || Now == Phase::Refused || Now == Phase::Failed);
  const char *Label = "Load into CS2";
  if (Now == Phase::Intro || Now == Phase::Checking) {
    Label = "Checking for updates";
  } else if (Now == Phase::Downloading || Now == Phase::Installing) {
    Label = "Updating";
  } else if (Now == Phase::Loading) {
    Label = "Loading";
  } else if (Now == Phase::Loaded) {
    Label = "Loaded";
  } else if (Now == Phase::Refused || Now == Phase::Failed) {
    Label = Pid == 0 ? "Waiting for CS2" : "Try again";
  } else if (Pid == 0) {
    Label = "Waiting for CS2";
  }
  const float Rise = easeOutCubic((static_cast<float>(Time) - 0.9f) / 0.5f);
  ImGui::SetCursorPos({32.0f, 270.0f + (1.0f - Rise) * 12.0f});
  ImGui::PushFont(Font.Button);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * Rise);
  ImGui::BeginDisabled(!CanLoad);
  if (Now == Phase::Loaded) {
    ImGui::PushStyleColor(ImGuiCol_Button, {0.10f, 0.32f, 0.20f, 1.0f});
  }
  if (ImGui::Button(Label, {Width - 64.0f, 52.0f})) {
    startLoading(Pid);
  }
  if (Now == Phase::Loaded) {
    ImGui::PopStyleColor();
  }
  pointerOnHover(!CanLoad);
  ImGui::SetItemDefaultFocus();
  if (Now == Phase::Loading) {
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const float Sweep = std::fmod(static_cast<float>(Time) * 1.4f, 1.0f);
    const float X = Min.x + (Max.x - Min.x) * Sweep;
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        {std::max(Min.x, X - 60.0f), Min.y}, {std::min(Max.x, X + 60.0f), Max.y},
        colour(Ink, 0.0f), colour(Ink, 0.12f), colour(Ink, 0.12f), colour(Ink, 0.0f));
  }
  ImGui::EndDisabled();
  ImGui::PopStyleVar();
  ImGui::PopFont();
}

void frame(HWND Window, const Fonts &Font, double Time) {
  static Phase Last = Phase::Intro;
  static double PhaseStart = 0.0;
  const Phase Now = currentPhase(Time);
  if (Now != Last) {
    Last = Now;
    PhaseStart = Time;
  }
  const double PhaseTime = Time - PhaseStart;

  if (Now == Phase::Installing && !UpdateFailed) {
    if (larp::applyUpdate(Self, Update.downloaded())) {
      PostQuitMessage(0);
    } else {
      UpdateFailed = true;
    }
  }

  ImGui::GetStyle().Alpha = std::max(0.01f, easeOutCubic(static_cast<float>(Time) / 0.3f));
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize({static_cast<float>(Width), static_cast<float>(Height)});
  ImGui::Begin("LarpChanger", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

  if (closeButton() || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    PostQuitMessage(0);
  }
  drawEmblem(Now, Time, PhaseTime);
  revealTitle(Font, Time);

  const float Sub = easeOutCubic((static_cast<float>(Time) - 0.8f) / 0.5f);
  const char *const Tagline = "Offline skin changer for CS2 bot matches";
  ImGui::GetWindowDrawList()->AddText(
      {ImGui::GetWindowPos().x + (Width - ImGui::CalcTextSize(Tagline).x) * 0.5f,
       ImGui::GetWindowPos().y + 196.0f},
      colour(Muted, Sub), Tagline);

  if (Time > 1.0) {
    statusLine(statusText(Now), statusColour(Now), Time);
  }
  actionButton(Font, Now, Time);

  char Footer[128];
  std::snprintf(Footer, sizeof(Footer), "v%s  \xC2\xB7  %s", LARP_VERSION,
                UpdateFailed || Update.Now == larp::Updater::State::Failed
                    ? "update check failed, running this version"
                    : "fork of OpenChanger, MIT License");
  ImGui::GetWindowDrawList()->AddText(
      {ImGui::GetWindowPos().x + (Width - ImGui::CalcTextSize(Footer).x) * 0.5f,
       ImGui::GetWindowPos().y + Height - 34.0f},
      colour(Muted, Sub * 0.85f), Footer);

  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ReleaseCapture();
    SendMessageW(Window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
  }
  ImGui::End();
}

void applyStyle() {
  ImGuiStyle &Style = ImGui::GetStyle();
  Style.WindowRounding = 0.0f;
  Style.WindowBorderSize = 0.0f;
  Style.WindowPadding = {0.0f, 0.0f};
  Style.FrameRounding = 12.0f;
  Style.DisabledAlpha = 0.55f;
  ImVec4 *Colours = Style.Colors;
  Colours[ImGuiCol_WindowBg] = Base;
  Colours[ImGuiCol_Text] = Ink;
  Colours[ImGuiCol_Button] = ButtonBlue;
  Colours[ImGuiCol_ButtonHovered] = {0.231f, 0.478f, 0.965f, 1.0f};
  Colours[ImGuiCol_ButtonActive] = {0.114f, 0.306f, 0.847f, 1.0f};
  Colours[ImGuiCol_NavCursor] = Ink;
}

Fonts loadFonts() {
  ImGuiIO &Io = ImGui::GetIO();
  Io.IniFilename = nullptr;
  Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  wchar_t Windows[MAX_PATH];
  GetWindowsDirectoryW(Windows, MAX_PATH);
  char Path[MAX_PATH * 2];
  WideCharToMultiByte(CP_UTF8, 0, Windows, -1, Path, sizeof(Path), nullptr, nullptr);
  const std::string Dir = std::string(Path) + "\\Fonts\\";
  const auto exists = [](const std::string &File) {
    return GetFileAttributesA(File.c_str()) != INVALID_FILE_ATTRIBUTES;
  };
  Fonts Font;
  if (!exists(Dir + "segoeui.ttf") || !exists(Dir + "segoeuib.ttf") ||
      !exists(Dir + "seguisb.ttf")) {
    Font.Body = Io.Fonts->AddFontDefault();
    Font.Title = Font.Button = Font.Body;
    return Font;
  }
  Font.Body = Io.Fonts->AddFontFromFileTTF((Dir + "segoeui.ttf").c_str(), 17.0f);
  Font.Title = Io.Fonts->AddFontFromFileTTF((Dir + "segoeuib.ttf").c_str(), 36.0f);
  Font.Button = Io.Fonts->AddFontFromFileTTF((Dir + "seguisb.ttf").c_str(), 18.0f);
  return Font;
}

bool createDevice(HWND Window) {
  DXGI_SWAP_CHAIN_DESC Desc{};
  Desc.BufferCount = 2;
  Desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  Desc.OutputWindow = Window;
  Desc.SampleDesc.Count = 1;
  Desc.Windowed = TRUE;
  Desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL Levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                           Levels, 2, D3D11_SDK_VERSION, &Desc, &SwapChain,
                                           &Device, nullptr, &Context))) {
    return false;
  }
  ID3D11Texture2D *Back = nullptr;
  SwapChain->GetBuffer(0, IID_PPV_ARGS(&Back));
  Device->CreateRenderTargetView(Back, nullptr, &Target);
  Back->Release();
  return true;
}

LRESULT CALLBACK windowProc(HWND Window, UINT Message, WPARAM W, LPARAM L) {
  if (ImGui_ImplWin32_WndProcHandler(Window, Message, W, L)) {
    return 1;
  }
  if (Message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(Window, Message, W, L);
}

} // namespace

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, PWSTR, int) {
  wchar_t Path[MAX_PATH];
  Self.assign(Path, GetModuleFileNameW(nullptr, Path, MAX_PATH));
  Installed = install();
  WNDCLASSEXW Class{.cbSize = sizeof(Class),
                    .style = CS_CLASSDC,
                    .lpfnWndProc = windowProc,
                    .hInstance = Instance,
                    .hIcon = LoadIconW(Instance, MAKEINTRESOURCEW(1)),
                    .hCursor = LoadCursorW(nullptr, IDC_ARROW),
                    .lpszClassName = L"LarpChanger"};
  RegisterClassExW(&Class);
  const int X = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
  const int Y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
  HWND Window = CreateWindowExW(0, Class.lpszClassName, L"LarpChanger",
                                WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, X, Y, Width, Height,
                                nullptr, nullptr, Instance, nullptr);
  const DWM_WINDOW_CORNER_PREFERENCE Round = DWMWCP_ROUND;
  DwmSetWindowAttribute(Window, DWMWA_WINDOW_CORNER_PREFERENCE, &Round, sizeof(Round));
  if (!createDevice(Window)) {
    MessageBoxW(nullptr, L"Direct3D 11 is not available.", L"LarpChanger", MB_ICONERROR);
    return 1;
  }
  ShowWindow(Window, SW_SHOWDEFAULT);
  UpdateWindow(Window);

  ImGui::CreateContext();
  const Fonts Font = loadFonts();
  applyStyle();
  ImGui_ImplWin32_Init(Window);
  ImGui_ImplDX11_Init(Device, Context);
  startBackground();

  LARGE_INTEGER Frequency;
  LARGE_INTEGER Start;
  QueryPerformanceFrequency(&Frequency);
  QueryPerformanceCounter(&Start);
  for (bool Running = true; Running;) {
    MSG Message;
    while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&Message);
      DispatchMessageW(&Message);
      Running = Running && Message.message != WM_QUIT;
    }
    if (!Running) {
      break;
    }
    LARGE_INTEGER Now;
    QueryPerformanceCounter(&Now);
    const double Time = static_cast<double>(Now.QuadPart - Start.QuadPart) /
                        static_cast<double>(Frequency.QuadPart);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::PushFont(Font.Body);
    frame(Window, Font, Time);
    ImGui::PopFont();
    ImGui::Render();
    const float Clear[4] = {Base.x, Base.y, Base.z, 1.0f};
    Context->OMSetRenderTargets(1, &Target, nullptr);
    Context->ClearRenderTargetView(Target, Clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    SwapChain->Present(1, 0);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  Target->Release();
  SwapChain->Release();
  Context->Release();
  Device->Release();
  DestroyWindow(Window);
  return 0;
}
