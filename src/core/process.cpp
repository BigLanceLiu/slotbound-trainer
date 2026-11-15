#include "process.h"

#include <tlhelp32.h>

namespace {

DWORD FindPid(const std::wstring& exeName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry{ sizeof(entry) };
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap, &entry); ok; ok = Process32NextW(snap, &entry)) {
        if (_wcsicmp(entry.szExeFile, exeName.c_str()) == 0) {
            pid = entry.th32ProcessID;
            break;
        }
    }
    CloseHandle(snap);
    return pid;
}

struct WindowSearch {
    DWORD pid;
    HWND found;
    LONG foundArea;
};

// 取这个进程面积最大的可见顶层窗口（LÖVE/SDL 只有一个游戏窗口，但启动时可能有别的小窗）
BOOL CALLBACK FindMainWindowProc(HWND hwnd, LPARAM lParam)
{
    auto* search = reinterpret_cast<WindowSearch*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
        return TRUE;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    const LONG area = (rect.right - rect.left) * (rect.bottom - rect.top);
    if (area > search->foundArea) {
        search->found = hwnd;
        search->foundArea = area;
    }
    return TRUE;
}

} // namespace

bool GameProcess::Attach(const std::wstring& exeName)
{
    Detach();

    const DWORD pid = FindPid(exeName);
    if (!pid)
        return false;

    // SYNCHRONIZE 判断进程是否已退出；QUERY_INFORMATION 枚举内存区域（定位 Lua 变量要扫堆）
    HANDLE handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!handle)
        return false;

    handle_ = handle;
    pid_ = pid;
    RefreshWindow();
    return true;
}

void GameProcess::Detach()
{
    if (writeHandle_)
        CloseHandle(writeHandle_);
    writeHandle_ = nullptr;
    if (handle_)
        CloseHandle(handle_);
    handle_ = nullptr;
    pid_ = 0;
    hwnd_ = nullptr;
}

bool GameProcess::IsAlive() const
{
    return handle_ && WaitForSingleObject(handle_, 0) == WAIT_TIMEOUT;
}

void GameProcess::RefreshWindow()
{
    if (!pid_) {
        hwnd_ = nullptr;
        return;
    }
    // 每次都重新找，不缓存：窗口可能晚出现
    WindowSearch search{ pid_, nullptr, 0 };
    EnumWindows(FindMainWindowProc, reinterpret_cast<LPARAM>(&search));
    hwnd_ = search.found;
}

bool GameProcess::ReadRaw(uintptr_t addr, void* out, size_t size) const
{
    SIZE_T read = 0;
    return handle_ && ReadProcessMemory(handle_, reinterpret_cast<LPCVOID>(addr), out, size, &read) && read == size;
}

bool GameProcess::EnableWrite()
{
    if (writeHandle_)
        return true;
    if (!pid_)
        return false;
    writeHandle_ = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid_);
    return writeHandle_ != nullptr;
}

bool GameProcess::WriteRaw(uintptr_t addr, const void* data, size_t size) const
{
    SIZE_T written = 0;
    return writeHandle_ && WriteProcessMemory(writeHandle_, reinterpret_cast<LPVOID>(addr), data, size, &written) && written == size;
}
