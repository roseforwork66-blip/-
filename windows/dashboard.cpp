#include "demo_scenarios.h"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <string_view>

namespace {

using cane::State;
using cane::Stop;
using cane::demo::Frame;
using cane::demo::Scenario;

constexpr wchar_t windowClass[] = L"SmartGuideCaneDashboard";
constexpr COLORREF page = RGB(239, 246, 247);
constexpr COLORREF paper = RGB(255, 255, 255);
constexpr COLORREF ink = RGB(19, 40, 50);
constexpr COLORREF subdued = RGB(76, 101, 111);
constexpr COLORREF navy = RGB(12, 31, 42);
constexpr COLORREF mint = RGB(116, 238, 202);
constexpr COLORREF orange = RGB(250, 179, 109);
constexpr UINT_PTR replayTimer = 1;
enum Command : WORD { scenarios = 200, play, next, reset };

struct Scene {
    Scenario type;
    const wchar_t* title;
    const wchar_t* note;
};

constexpr std::array<Scene, 5> scenes{{
    {Scenario::Straight, L"前方通畅", L"前方路径畅通，控制程序给出直行指令。"},
    {Scenario::Left, L"左侧可绕", L"正前方遇到障碍，控制程序选择从左侧通过。"},
    {Scenario::Right, L"右侧可绕", L"正前方遇到障碍，控制程序选择从右侧通过。"},
    {Scenario::Blocked, L"前方阻塞", L"通行路线受阻，程序关闭舵机输出并提示停步。"},
    {Scenario::ScanLoss, L"雷达超时", L"模拟扫描数据超时，程序关闭舵机输出并提示停步。"},
}};

const wchar_t* stateText(State state) {
    switch (state) {
        case State::Idle: return L"待命";
        case State::Straight: return L"直行";
        case State::Sidestep: return L"侧向绕行";
        case State::Pass: return L"通过障碍";
        case State::Stopped: return L"提示停步";
    }
    return L"状态未知";
}

const wchar_t* reasonText(Stop reason) {
    switch (reason) {
        case Stop::None: return L"—";
        case Stop::Obstacle: return L"检测到障碍";
        case Stop::ScanStale: return L"雷达数据超时";
        default: return L"控制保护已触发";
    }
}

void fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void label(HDC dc, RECT rect, HFONT font, COLORREF color, const wchar_t* text, UINT flags) {
    const HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, flags | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

void drawRoute(HDC dc, const RECT& area, Scenario scenario, State state) {
    fill(dc, area, navy);
    const auto point = [&area](double x, double y) {
        return POINT{area.left + static_cast<LONG>((area.right - area.left) * x),
                     area.top + static_cast<LONG>((area.bottom - area.top) * y)};
    };
    const auto line = [dc](POINT a, POINT b) {
        MoveToEx(dc, a.x, a.y, nullptr);
        LineTo(dc, b.x, b.y);
    };
    const auto obstacle = [dc, &point](double left, double right, double top, double bottom) {
        const POINT a = point(left, top);
        const POINT b = point(right, bottom);
        fill(dc, RECT{a.x, a.y, b.x, b.y}, RGB(157, 101, 64));
    };

    HPEN grid = CreatePen(PS_SOLID, 1, RGB(42, 70, 81));
    const HGDIOBJ originalPen = SelectObject(dc, grid);
    for (int i = 1; i < 10; ++i) {
        const double fraction = i * 0.1;
        line(point(fraction, 0), point(fraction, 1));
        line(point(0, fraction), point(1, fraction));
    }
    const bool detour = scenario == Scenario::Left || scenario == Scenario::Right;
    const double side = scenario == Scenario::Left ? 0.27 : 0.73;
    if (detour) {
        obstacle(0.43, 0.57, 0.37, 0.49);
        if (scenario == Scenario::Left) {
            obstacle(0.64, 0.93, 0.54, 0.62);
        } else {
            obstacle(0.07, 0.36, 0.54, 0.62);
        }
    } else if (scenario == Scenario::Blocked) {
        obstacle(0.13, 0.87, 0.40, 0.52);
    }

    const COLORREF color = state == State::Stopped ? orange : mint;
    HPEN route = CreatePen(PS_SOLID, 3, state == State::Idle ? RGB(97, 153, 144) : color);
    SelectObject(dc, route);
    if (detour) {
        const POINT path[]{point(0.50, 0.82), point(0.50, 0.73), point(side, 0.54), point(side, 0.16)};
        Polyline(dc, path, 4);
    } else {
        line(point(0.50, 0.82), point(0.50, scenario == Scenario::Blocked ? 0.64 : 0.16));
    }
    POINT marker = point(0.5, 0.82);
    switch (state) {
        case State::Idle: break;
        case State::Straight: marker = point(detour ? side : 0.5, 0.20); break;
        case State::Sidestep: marker = point(side, 0.54); break;
        case State::Pass: marker = point(side, 0.34); break;
        case State::Stopped: marker = point(0.5, 0.66); break;
    }
    HBRUSH dot = CreateSolidBrush(color);
    const HGDIOBJ originalBrush = SelectObject(dc, dot);
    Ellipse(dc, marker.x - 5, marker.y - 5, marker.x + 6, marker.y + 6);
    SelectObject(dc, originalBrush);
    SelectObject(dc, originalPen);
    DeleteObject(dot);
    DeleteObject(route);
    DeleteObject(grid);
}

struct Dashboard {
    HWND window{};
    HWND selectorLabel{}, selector{}, playButton{}, nextButton{}, resetButton{}, status{}, metrics{};
    HBRUSH paperBrush = CreateSolidBrush(paper);
    HBRUSH pageBrush = CreateSolidBrush(page);
    HFONT titleFont{}, bodyFont{}, headingFont{}, stateFont{};
    UINT dpi = 96;
    RECT map{}, info{};
    std::vector<Frame> frames;
    std::size_t scene = 1;
    std::size_t index = 0;
    bool playing = false;

    ~Dashboard() {
        DeleteObject(titleFont);
        DeleteObject(bodyFont);
        DeleteObject(headingFont);
        DeleteObject(stateFont);
        DeleteObject(paperBrush);
        DeleteObject(pageBrush);
    }

    int px(int units) const { return MulDiv(units, static_cast<int>(dpi), 96); }

    HFONT font(int size, int weight) const {
        return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, L"Microsoft YaHei UI");
    }

    HWND control(const wchar_t* type, const wchar_t* text, DWORD style, WORD id) const {
        return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                               window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                               reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE)), nullptr);
    }

    bool create() {
        dpi = GetDpiForWindow(window);
        titleFont = font(32, FW_BOLD);
        bodyFont = font(16, FW_NORMAL);
        headingFont = font(18, FW_BOLD);
        stateFont = font(27, FW_BOLD);
        selectorLabel = control(L"STATIC", L"选择场景", SS_LEFT, 0);
        selector = control(L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, scenarios);
        playButton = control(L"BUTTON", L"播放", WS_TABSTOP | BS_DEFPUSHBUTTON, play);
        nextButton = control(L"BUTTON", L"下一帧", WS_TABSTOP | BS_PUSHBUTTON, next);
        resetButton = control(L"BUTTON", L"重置", WS_TABSTOP | BS_PUSHBUTTON, reset);
        status = control(L"STATIC", L"", SS_LEFT, 0);
        metrics = control(L"STATIC", L"", SS_LEFT, 0);
        if (!selectorLabel || !selector || !playButton || !nextButton || !resetButton || !status || !metrics) {
            return false;
        }
        for (HWND child : {selectorLabel, selector, playButton, nextButton, resetButton, metrics}) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont), FALSE);
        }
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(stateFont), FALSE);
        for (const Scene& choice : scenes) {
            SendMessageW(selector, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.title));
        }
        SendMessageW(selector, CB_SETCURSEL, scene, 0);
        selectScene(scene);
        return true;
    }

    void layout(int width, int height) {
        const int margin = px(26);
        const int gap = px(16);
        const int infoWidth = px(232);
        const int buttons = px(96);
        MoveWindow(selectorLabel, margin, px(118), px(80), px(25), TRUE);
        MoveWindow(selector, margin + px(82), px(112), px(185), px(230), TRUE);
        MoveWindow(playButton, width - margin - 3 * buttons - 2 * gap, px(109), buttons, px(34), TRUE);
        MoveWindow(nextButton, width - margin - 2 * buttons - gap, px(109), buttons, px(34), TRUE);
        MoveWindow(resetButton, width - margin - buttons, px(109), buttons, px(34), TRUE);
        info = {width - margin - infoWidth, px(164), width - margin, height - px(56)};
        map = {margin, info.top, info.left - gap, info.bottom};
        MoveWindow(status, info.left + px(16), info.top + px(52), infoWidth - px(32), px(44), TRUE);
        MoveWindow(metrics, info.left + px(16), info.top + px(103), infoWidth - px(32), px(125), TRUE);
        InvalidateRect(window, nullptr, FALSE);
    }

    void stopPlayback() {
        KillTimer(window, replayTimer);
        playing = false;
    }

    void refresh() const {
        const Frame& frame = frames[index];
        const auto& output = frame.output;
        SetWindowTextW(status, stateText(output.state));
        wchar_t values[256]{};
        std::swprintf(values, std::size(values), L"仿真时间：%u ms\r\n帧序号：%zu / %zu\r\n舵机输出：%ls\r\n目标角度：%.2f°\r\n停步原因：%ls",
                      frame.milliseconds, index + 1, frames.size(), output.steeringEnabled ? L"开启" : L"关闭",
                      static_cast<double>(output.steeringDeg), reasonText(output.stop));
        SetWindowTextW(metrics, values);
        SetWindowTextW(playButton, playing ? L"暂停" : L"播放");
        EnableWindow(nextButton, index + 1 < frames.size());
        InvalidateRect(window, nullptr, FALSE);
        InvalidateRect(status, nullptr, TRUE);
    }

    void selectScene(std::size_t choice) {
        stopPlayback();
        scene = choice;
        index = 0;
        frames = cane::demo::run(scenes[scene].type);
        refresh();
    }

    void advance() {
        if (index + 1 < frames.size()) {
            ++index;
        }
        if (index + 1 == frames.size()) {
            stopPlayback();
        }
        refresh();
    }

    void interact(WORD id, WORD message) {
        if (id == scenarios && message == CBN_SELCHANGE) {
            const LRESULT selected = SendMessageW(selector, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && static_cast<std::size_t>(selected) < scenes.size()) {
                selectScene(static_cast<std::size_t>(selected));
            }
        } else if (id == play && message == BN_CLICKED) {
            if (playing) {
                stopPlayback();
            } else {
                if (index + 1 == frames.size()) {
                    index = 0;
                }
                playing = SetTimer(window, replayTimer, 950, nullptr) != 0;
            }
            refresh();
        } else if (id == next && message == BN_CLICKED) {
            stopPlayback();
            advance();
        } else if (id == reset && message == BN_CLICKED) {
            stopPlayback();
            index = 0;
            refresh();
        }
    }

    void draw(HDC dc, const RECT& client) const {
        FillRect(dc, &client, pageBrush);
        const int left = px(26);
        label(dc, {left, px(19), client.right - left, px(57)}, titleFont, ink, L"双轮式智能导盲杖", DT_SINGLELINE);
        label(dc, {left, px(65), client.right - left, px(95)}, bodyFont, subdued,
              L"离线仿真 · 五个场景展示控制程序的转向与停步过程", DT_SINGLELINE);

        FillRect(dc, &map, paperBrush);
        FillRect(dc, &info, paperBrush);
        label(dc, {map.left + px(16), map.top + px(12), map.right - px(16), map.top + px(42)},
              headingFont, ink, L"路线示意", DT_SINGLELINE);
        label(dc, {info.left + px(16), info.top + px(12), info.right - px(16), info.top + px(42)},
              headingFont, ink, L"控制程序输出", DT_SINGLELINE);
        const RECT routeArea{map.left + px(12), map.top + px(43), map.right - px(12), map.bottom - px(12)};
        drawRoute(dc, routeArea, scenes[scene].type, frames[index].output.state);
        label(dc, {left, client.bottom - px(38), client.right - left, client.bottom - px(12)},
              bodyFont, subdued, scenes[scene].note, DT_SINGLELINE | DT_END_ELLIPSIS);
    }
};

LRESULT CALLBACK process(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<Dashboard*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<Dashboard*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    switch (message) {
        case WM_CREATE: return app->create() ? 0 : -1;
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                app->layout(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* sizes = reinterpret_cast<MINMAXINFO*>(lParam);
            sizes->ptMinTrackSize = {app->px(676), app->px(494)};
            return 0;
        }
        case WM_COMMAND: app->interact(LOWORD(wParam), HIWORD(wParam)); return 0;
        case WM_TIMER:
            if (wParam == replayTimer && app->playing) {
                app->advance();
            }
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND child = reinterpret_cast<HWND>(lParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, child == app->status && !app->frames.empty() &&
                                 app->frames[app->index].output.state == State::Stopped
                                 ? RGB(159, 73, 21) : ink);
            return reinterpret_cast<LRESULT>(child == app->selectorLabel ? app->pageBrush : app->paperBrush);
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            if (!app->frames.empty()) {
                app->draw(dc, client);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY: app->stopPlayback(); PostQuitMessage(0); return 0;
        default: return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool smokeTest(const Dashboard& app) {
    const auto command = [&app](WORD id, WORD action) { SendMessageW(app.window, WM_COMMAND, MAKEWPARAM(id, action), 0); };
    const std::array<std::size_t, 5> counts{{2, 4, 4, 2, 3}};
    const std::array<Stop, 5> stops{{Stop::None, Stop::None, Stop::None, Stop::Obstacle, Stop::ScanStale}};
    for (std::size_t i = 0; i < scenes.size(); ++i) {
        SendMessageW(app.selector, CB_SETCURSEL, i, 0);
        command(scenarios, CBN_SELCHANGE);
        if (app.scene != i || app.frames.size() != counts[i] || app.frames.front().output.state != State::Idle ||
            app.frames.back().output.stop != stops[i] || app.frames.back().output.steeringEnabled != (i < 3)) {
            return false;
        }
        if ((i == 1 && app.frames[1].output.steeringDeg <= 0) || (i == 2 && app.frames[1].output.steeringDeg >= 0)) {
            return false;
        }
        command(next, BN_CLICKED);
        if (app.index != 1) {
            return false;
        }
        command(reset, BN_CLICKED);
        if (app.index != 0) {
            return false;
        }
    }
    command(play, BN_CLICKED);
    SendMessageW(app.window, WM_TIMER, replayTimer, 0);
    SendMessageW(app.window, WM_TIMER, replayTimer, 0);
    wchar_t visibleStatus[32]{};
    GetWindowTextW(app.status, visibleStatus, static_cast<int>(std::size(visibleStatus)));
    return !app.playing && app.index == 2 && !IsWindowEnabled(app.nextButton) &&
           std::wstring_view(visibleStatus) == L"提示停步";
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int show) {
    const bool smoke = std::wstring_view(commandLine) == L"--smoke-test";
    SetProcessDPIAware();
    const INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    Dashboard app;
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.lpfnWndProc = process;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    type.hIconSm = type.hIcon;
    type.lpszClassName = windowClass;
    if (!RegisterClassExW(&type)) {
        return 1;
    }
    const UINT dpi = GetDpiForSystem();
    const int width = MulDiv(850, static_cast<int>(dpi), 96);
    const int preferredHeight = MulDiv(660, static_cast<int>(dpi), 96);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int height = work.bottom > work.top
        ? std::min(preferredHeight, static_cast<int>(work.bottom - work.top)) : preferredHeight;
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, windowClass, L"双轮式智能导盲杖 · Windows 演示",
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, width,
                                  height, nullptr, nullptr, instance, &app);
    if (!window) {
        return 1;
    }
    if (smoke) {
        const bool passed = smokeTest(app);
        DestroyWindow(window);
        return passed ? 0 : 1;
    }
    ShowWindow(window, show);
    SetFocus(app.selector);
    MSG event{};
    BOOL received;
    while ((received = GetMessageW(&event, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(window, &event)) {
            TranslateMessage(&event);
            DispatchMessageW(&event);
        }
    }
    return received == -1 ? 1 : static_cast<int>(event.wParam);
}
