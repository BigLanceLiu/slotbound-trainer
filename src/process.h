#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

// 外部读写游戏内存。默认只申请读权限；写权限要显式 EnableWrite()，
// 只有真的要改数值（改金币、锁金币）时才申请。不注入、不改代码段、不改游戏文件。
class GameProcess {
public:
    GameProcess() = default;
    GameProcess(const GameProcess&) = delete;
    GameProcess& operator=(const GameProcess&) = delete;
    ~GameProcess() { Detach(); }

    // 按 exe 名找进程并打开句柄。找不到返回 false。
    bool Attach(const std::wstring& exeName);
    void Detach();
    bool IsAlive() const;

    // 游戏主窗口可能比进程晚出现，所以单独定时刷新（调用方每秒一次）。
    void RefreshWindow();

    DWORD Pid() const { return pid_; }
    HANDLE Handle() const { return handle_; } // 只读句柄，枚举内存区域用（VirtualQueryEx）
    HWND Window() const { return hwnd_; }

    bool ReadRaw(uintptr_t addr, void* out, size_t size) const;

    template <typename T>
    T Read(uintptr_t addr) const
    {
        T value{};
        ReadRaw(addr, &value, sizeof(T));
        return value;
    }

    // 另开一个带写权限的句柄；读仍然走只读句柄
    bool EnableWrite();
    bool CanWrite() const { return writeHandle_ != nullptr; }

    bool WriteRaw(uintptr_t addr, const void* data, size_t size) const;

    template <typename T>
    bool Write(uintptr_t addr, const T& value) const
    {
        return WriteRaw(addr, &value, sizeof(T));
    }

private:
    HANDLE handle_ = nullptr;
    HANDLE writeHandle_ = nullptr;
    DWORD pid_ = 0;
    HWND hwnd_ = nullptr;
};
