#include "loader.h"

#include <d3d11.h>
#include <dwmapi.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <atomic>
#include <cmath>
#include <string>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
                                                             LPARAM);

namespace {

constexpr int Width = 440;
constexpr int Height = 340;
constexpr int DllResource = 101;
constexpr int WelcomeResource = 102;
constexpr int WelcomeBackResource = 103;

ID3D11Device *Device = nullptr;
ID3D11DeviceContext *Context = nullptr;
IDXGISwapChain *SwapChain = nullptr;
ID3D11RenderTargetView *Target = nullptr;

enum class State { Idle, Loading, Loaded, Refused, Failed };
std::atomic<State> Status{State::Idle};
std::atomic<DWORD> GamePid{0};
std::wstring Folder;
bool Installed = false;

const ImVec4 Accent{0.231f, 0.510f, 0.965f, 1.0f};
const ImVec4 Muted{0.55f, 0.58f, 0.64f, 1.0f};
const ImVec4 Good{0.30f, 0.80f, 0.50f, 1.0f};
const ImVec4 Bad{0.95f, 0.38f, 0.38f, 1.0f};

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
  Status = State::Loading;
  std::thread([Pid] {
    const larp::Result Result =
        larp::inject(Pid, Folder + L"\\larpchanger.dll");
    Status = Result == larp::Result::Loaded    ? State::Loaded
             : Result == larp::Result::Refused ? State::Refused
                                               : State::Failed;
    if (Result == larp::Result::Loaded) {
      larp::playWelcome(Folder + L"\\sounds");
    }
  }).detach();
}

void watchGame() {
  std::thread([] {
    for (;;) {
      const DWORD Pid = larp::findCs2();
      if (Pid != GamePid.exchange(Pid) && Status != State::Loading) {
        Status = State::Idle;
      }
      Sleep(1000);
    }
  }).detach();
}

void drawStar(ImDrawList *List, ImVec2 Centre, float Radius, ImU32 Colour,
              float Thickness) {
  for (const float Offset : {-90.0f, 90.0f}) {
    ImVec2 Points[3];
    for (int Each = 0; Each < 3; ++Each) {
      const float Angle = (Offset + 120.0f * Each) * 3.14159265f / 180.0f;
      Points[Each] = {Centre.x + Radius * std::cos(Angle),
                      Centre.y + Radius * std::sin(Angle)};
    }
    List->AddPolyline(Points, 3, Colour, ImDrawFlags_Closed, Thickness);
  }
}

void centredText(const char *Text, ImVec4 Colour) {
  const float TextWidth = ImGui::CalcTextSize(Text).x;
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - TextWidth) * 0.5f);
  ImGui::TextColored(Colour, "%s", Text);
}

void applyStyle() {
  ImGuiStyle &Style = ImGui::GetStyle();
  Style.WindowRounding = 0.0f;
  Style.WindowBorderSize = 0.0f;
  Style.FrameRounding = 10.0f;
  Style.FramePadding = {14.0f, 12.0f};
  ImVec4 *Colours = Style.Colors;
  Colours[ImGuiCol_WindowBg] = {0.059f, 0.067f, 0.082f, 1.0f};
  Colours[ImGuiCol_Text] = {0.93f, 0.94f, 0.96f, 1.0f};
  Colours[ImGuiCol_Button] = Accent;
  Colours[ImGuiCol_ButtonHovered] = {0.36f, 0.60f, 0.98f, 1.0f};
  Colours[ImGuiCol_ButtonActive] = {0.18f, 0.42f, 0.86f, 1.0f};
}

void frame(HWND Window, ImFont *Title) {
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize({static_cast<float>(Width), static_cast<float>(Height)});
  ImGui::Begin("LarpChanger", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  ImDrawList *const List = ImGui::GetWindowDrawList();

  ImGui::SetCursorPos({Width - 40.0f, 10.0f});
  ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1, 1, 1, 0.08f});
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {9.0f, 5.0f});
  if (ImGui::Button("x")) {
    PostQuitMessage(0);
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  const float Pulse = 0.85f + 0.15f * std::sin(static_cast<float>(ImGui::GetTime()) * 2.0f);
  const ImU32 Glow = ImGui::GetColorU32({Accent.x, Accent.y, Accent.z, 0.18f * Pulse});
  List->AddCircleFilled({Width * 0.5f, 70.0f}, 46.0f, Glow, 48);
  drawStar(List, {Width * 0.5f, 70.0f}, 34.0f, ImGui::GetColorU32(Accent), 4.0f);

  ImGui::SetCursorPosY(124.0f);
  ImGui::PushFont(Title);
  centredText("LarpChanger", {0.96f, 0.97f, 0.98f, 1.0f});
  ImGui::PopFont();
  centredText("Offline use only  -  bots, CS2 started with -insecure", Muted);

  ImGui::SetCursorPosY(196.0f);
  const DWORD Pid = GamePid;
  const State Now = Status;
  if (!Installed) {
    centredText("Could not unpack LarpChanger", Bad);
  } else if (Now == State::Loaded) {
    centredText("Loaded. Open the new tab in CS2's main menu.", Good);
  } else if (Now == State::Refused) {
    centredText("Refused: start CS2 with -insecure, then try again.", Bad);
  } else if (Now == State::Failed) {
    centredText("Could not load into CS2.", Bad);
  } else if (Pid == 0) {
    centredText("Waiting for CS2...", Muted);
  } else {
    centredText("CS2 is running", Good);
  }

  ImGui::SetCursorPos({32.0f, 234.0f});
  const bool Ready = Installed && Pid != 0 && Now != State::Loading && Now != State::Loaded;
  ImGui::BeginDisabled(!Ready);
  const char *const Label = Now == State::Loading  ? "Loading..."
                            : Now == State::Loaded ? "Loaded"
                                                   : "Load into CS2";
  if (ImGui::Button(Label, {Width - 64.0f, 0.0f})) {
    startLoading(Pid);
  }
  ImGui::EndDisabled();

  ImGui::SetCursorPosY(Height - 34.0f);
  centredText("Fork of OpenChanger  -  MIT License", {0.38f, 0.40f, 0.45f, 1.0f});

  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ReleaseCapture();
    SendMessageW(Window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
  }
  ImGui::End();
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
  if (FAILED(D3D11CreateDeviceAndSwapChain(
          nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, Levels, 2,
          D3D11_SDK_VERSION, &Desc, &SwapChain, &Device, nullptr, &Context))) {
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

ImFont *loadFonts() {
  ImGuiIO &Io = ImGui::GetIO();
  Io.IniFilename = nullptr;
  wchar_t Windows[MAX_PATH];
  GetWindowsDirectoryW(Windows, MAX_PATH);
  char Fonts[MAX_PATH * 2];
  WideCharToMultiByte(CP_UTF8, 0, Windows, -1, Fonts, sizeof(Fonts), nullptr, nullptr);
  const std::string Regular = std::string(Fonts) + "\\Fonts\\segoeui.ttf";
  const std::string Bold = std::string(Fonts) + "\\Fonts\\segoeuib.ttf";
  if (GetFileAttributesA(Regular.c_str()) == INVALID_FILE_ATTRIBUTES ||
      GetFileAttributesA(Bold.c_str()) == INVALID_FILE_ATTRIBUTES) {
    Io.Fonts->AddFontDefault();
    return Io.Fonts->AddFontDefault();
  }
  Io.Fonts->AddFontFromFileTTF(Regular.c_str(), 17.0f);
  return Io.Fonts->AddFontFromFileTTF(Bold.c_str(), 32.0f);
}

} // namespace

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, PWSTR, int) {
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
                                WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, X, Y,
                                Width, Height, nullptr, nullptr, Instance, nullptr);
  const DWM_WINDOW_CORNER_PREFERENCE Round = DWMWCP_ROUND;
  DwmSetWindowAttribute(Window, DWMWA_WINDOW_CORNER_PREFERENCE, &Round, sizeof(Round));
  if (!createDevice(Window)) {
    MessageBoxW(nullptr, L"Direct3D 11 is not available.", L"LarpChanger", MB_ICONERROR);
    return 1;
  }
  ShowWindow(Window, SW_SHOWDEFAULT);
  UpdateWindow(Window);

  ImGui::CreateContext();
  ImFont *const Title = loadFonts();
  applyStyle();
  ImGui_ImplWin32_Init(Window);
  ImGui_ImplDX11_Init(Device, Context);
  watchGame();

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
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    frame(Window, Title);
    ImGui::Render();
    const float Clear[4] = {0.059f, 0.067f, 0.082f, 1.0f};
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
