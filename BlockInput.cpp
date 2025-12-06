#include <windows.h>
#include <shellapi.h>   // 托盘图标所需
#include <string>

// 函数声明
void UnblockInput();
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);

// 全局变量
HHOOK keyboardHook = NULL; // 键盘钩子句柄
HHOOK mouseHook = NULL;    // 鼠标钩子句柄
bool isBlocked = false;    // 输入屏蔽状态
HWND g_hwnd = NULL;        // 主窗口句柄，供其他函数使用
NOTIFYICONDATA nid = {};   // 托盘图标数据

// 解除输入屏蔽
void UnblockInput() {
    if (isBlocked) {
        isBlocked = false;
        std::wstring newText = L"Input Unblocked! (Ctrl+Alt+B to toggle)";
        SetWindowText(g_hwnd, newText.c_str());
        InvalidateRect(g_hwnd, NULL, TRUE); // 强制重绘窗口
    }
}

// 屏蔽输入
void BlockInput() {
    if (!isBlocked) {
        isBlocked = true;
        std::wstring newText = L"Input Blocked! (Ctrl+Alt+B to toggle)";
        SetWindowText(g_hwnd, newText.c_str());
        InvalidateRect(g_hwnd, NULL, TRUE);
    }
}

// 切换屏蔽状态
void ToggleBlockInput() {
    if (isBlocked)
        UnblockInput();
    else
        BlockInput();
}

// 添加托盘图标
void AddTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;           // 自定义托盘消息
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // 使用默认应用程序图标
    wcscpy_s(nid.szTip, L"Input Blocker");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// 移除托盘图标
void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

// 键盘钩子过程（低级钩子）
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        // 检测 Ctrl+Alt+Q（强制解除屏蔽）
        if (p->vkCode == 'Q' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            if (wParam == WM_KEYDOWN) {
                UnblockInput();
                return 1; // 阻止该键继续传递
            }
        }

        // 检测 Ctrl+Alt+B（切换屏蔽状态）
        if (p->vkCode == 'B' && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            if (wParam == WM_KEYDOWN) {
                ToggleBlockInput();
                return 1; // 阻止该键继续传递
            }
        }

        // 如果当前处于屏蔽状态，则阻止绝大多数按键，仅允许修饰键通过
        if (isBlocked) {
            if (p->vkCode == VK_CONTROL || p->vkCode == VK_LCONTROL || p->vkCode == VK_RCONTROL ||
                p->vkCode == VK_MENU || p->vkCode == VK_LMENU || p->vkCode == VK_RMENU ||
                p->vkCode == VK_SHIFT || p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT) {
                return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
            }
            else {
                return 1; // 屏蔽其他所有按键
            }
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// 鼠标钩子过程（低级钩子）
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (isBlocked && nCode >= 0) {
        return 1; // 屏蔽所有鼠标输入
    }
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

// 程序入口点
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"FloatingWindowClass";

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    // 不设置背景刷，因为我们自己绘制
    RegisterClass(&wc);

    // 创建悬浮窗口（置顶、分层、不显示在任务栏）
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"Press Ctrl+Alt+B to toggle input block",
        WS_POPUP,
        10, 10, 400, 50,            // 宽度加大，确保状态文本能完整显示
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 1;

    g_hwnd = hwnd; // 保存全局窗口句柄

    // 设置窗口整体透明度（200 = 半透明），不使用颜色键
    SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);

    // 安装低级钩子
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
    if (!keyboardHook || !mouseHook) {
        MessageBox(hwnd, L"Hook installation failed", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 添加托盘图标
    AddTrayIcon(hwnd);

    // 主消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    if (mouseHook)    UnhookWindowsHookEx(mouseHook);
    RemoveTrayIcon();

    return 0;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 用黑色填充整个客户区（由于设置了 LWA_ALPHA，会呈现半透明效果）
        // 填充背景可以避免文字残留
        FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));

        // 绘制窗口标题文本（居中、白色、透明背景）
        RECT rect;
        GetClientRect(hwnd, &rect);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        wchar_t buffer[256];
        GetWindowText(hwnd, buffer, 256);
        DrawText(hdc, buffer, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_USER + 1: // 托盘图标消息
        if (lParam == WM_RBUTTONUP) {
            // 弹出右键菜单（仅包含“退出”选项）
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, 1001, L"Exit");

            POINT pt;
            GetCursorPos(&pt);

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001) // 点击了“退出”
            PostQuitMessage(0);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
