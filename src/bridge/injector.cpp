#include "injector.h"

#include "protocol.h"

namespace bridge {

bool Injected()
{
    HANDLE e = OpenEventW(SYNCHRONIZE, FALSE, kUnloadEvent);
    if (!e)
        return false;
    CloseHandle(e);
    return true;
}

bool Inject(DWORD pid, std::wstring dll, std::string& error)
{
    if (dll.empty()) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        dll = path;
        dll.resize(dll.find_last_of(L"\\/") + 1);
        dll += L"slotbound_lua.dll";
    }
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = "找不到 slotbound_lua.dll（要和修改器放在同一个文件夹）";
        return false;
    }
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ |
            PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!proc) {
        error = "打不开游戏进程";
        return false;
    }
    const size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool ok = false;
    error = "写入 DLL 路径失败";
    if (remote && WriteProcessMemory(proc, remote, dll.c_str(), bytes, nullptr)) {
        auto loadLibrary =
            reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        error = "创建远程线程失败";
        if (HANDLE t = CreateRemoteThread(proc, nullptr, 0, loadLibrary, remote, 0, nullptr)) {
            DWORD code = 0;
            ok = WaitForSingleObject(t, 5000) == WAIT_OBJECT_0 && GetExitCodeThread(t, &code) && code;
            error = ok ? "" : "LoadLibrary 失败或超时";
            CloseHandle(t);
        }
    }
    if (remote)
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return ok;
}

bool Unload()
{
    HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, kUnloadEvent);
    if (!e)
        return false;
    SetEvent(e);
    CloseHandle(e);
    return true;
}

} // namespace bridge
