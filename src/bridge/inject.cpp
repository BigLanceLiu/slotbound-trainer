// slotbound_inject.exe：把 slotbound_lua.dll 注入 / 卸出游戏。
//   slotbound_inject.exe load     注入 exe 同目录的 slotbound_lua.dll
//   slotbound_inject.exe unload   通知 DLL 自己恢复导入表并卸载
#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <string>

#include "injector.h"

static DWORD FindGame()
{
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{ sizeof(pe) };
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"Slotbound.exe") == 0)
            pid = pe.th32ProcessID;
    CloseHandle(snap);
    return pid;
}

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);
    const std::wstring cmd = argc > 1 ? argv[1] : L"";
    if (cmd == L"unload") {
        puts(bridge::Unload() ? "已通知 slotbound_lua.dll 卸载" : "slotbound_lua.dll 没在运行");
        return 0;
    }
    if (cmd != L"load") {
        puts("用法: slotbound_inject load | unload");
        return 2;
    }
    if (bridge::Injected()) {
        puts("已经注入过了");
        return 0;
    }
    const DWORD pid = FindGame();
    if (!pid) {
        puts("Slotbound.exe 没在运行");
        return 1;
    }
    std::string error;
    if (!bridge::Inject(pid, L"", error)) {
        printf("注入失败：%s\n", error.c_str());
        return 1;
    }
    printf("已注入 pid=%lu\n", pid);
    return 0;
}
