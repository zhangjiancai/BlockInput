#include <windows.h>
#include <iostream>

// 声明 UnblockInput 函数
void UnblockInput();

HHOOK keyboardHook = NULL;
HHOOK mouseHook = NULL;
bool isBlocked = false;
HANDLE exitEvent = NULL; // 用于通知退出的事件

// 键盘钩子过程
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        // 检查是否是解除屏蔽的快捷键 (Ctrl + Alt + Q)
        if (p->vkCode == 'Q' && GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState(VK_MENU) & 0x8000) {
            if (wParam == WM_KEYDOWN) {
                UnblockInput();
                return 1; // 阻止该键的默认处理
            }
        }

        if (isBlocked) {
            // 允许修饰键通过
            if (p->vkCode == VK_CONTROL || p->vkCode == VK_LCONTROL || p->vkCode == VK_RCONTROL ||
                p->vkCode == VK_MENU || p->vkCode == VK_LMENU || p->vkCode == VK_RMENU ||
                p->vkCode == VK_SHIFT || p->vkCode == VK_LSHIFT || p->vkCode == VK_RSHIFT) {
                return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
            }
            else {
                return 1; // 阻止其他所有按键
            }
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// 鼠标钩子过程
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (isBlocked && nCode >= 0) {
        return 1; // 如果处于屏蔽状态，阻止所有鼠标输入
    }
    return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

void UnblockInput() {
    if (isBlocked) {
        isBlocked = false;
        std::cout << "输入已解除屏蔽！" << std::endl;
        SetEvent(exitEvent); // 设置事件，通知主线程退出
    }
}

void BlockInputWithHook() {
    if (!isBlocked) {
        isBlocked = true;
        std::cout << "键盘和鼠标输入已屏蔽！" << std::endl;
        std::cout << "按 Ctrl + Alt + Q 解除屏蔽。" << std::endl;
    }
}

int main() {
    // 创建退出事件
    exitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!exitEvent) {
        std::cerr << "无法创建退出事件！" << std::endl;
        return 1;
    }

    // 安装键盘和鼠标钩子
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

    if (!keyboardHook || !mouseHook) {
        std::cerr << "无法安装钩子！" << std::endl;
        CloseHandle(exitEvent);
        return 1;
    }

    // 开始屏蔽输入
    BlockInputWithHook();

    // 使用 MsgWaitForMultipleObjects 的消息循环
    MSG msg;
    while (true) {
        DWORD result = MsgWaitForMultipleObjects(
            1,                  // 等待的对象数量
            &exitEvent,        // 等待的事件
            FALSE,             // 不需要所有对象都触发
            INFINITE,          // 无限等待
            QS_ALLINPUT        // 等待所有类型的输入消息
        );

        if (result == WAIT_OBJECT_0) {
            // 退出事件被触发，退出循环
            break;
        }
        else if (result == WAIT_OBJECT_0 + 1) {
            // 有消息到达，处理所有消息
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else {
            // 错误发生，退出循环
            std::cerr << "消息循环出错！" << std::endl;
            break;
        }
    }

    // 卸载钩子
    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);

    // 关闭退出事件
    if (exitEvent) CloseHandle(exitEvent);

    std::cout << "程序结束。" << std::endl;

    // 添加关闭窗口的代码
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }

    return 0;
}
