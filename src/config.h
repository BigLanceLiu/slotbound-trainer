#pragma once

#include <windows.h>

#include <string>

// 一个可改数值（金币、旋转次数）的设置，对应 ini 里的一段
struct VarConfig {
    bool lock = false;  // 锁定：用掉了也会写回锁定值
    int lockValue = 0;  // 锁定在多少
    int addAmount = 0;  // 「加」快捷键每次加多少
    int lockKey = 0;    // 开关锁定的快捷键；0 = 不绑
    int addKey = 0;     // 加的快捷键；0 = 不绑

    bool operator==(const VarConfig&) const = default;
};

// 对应 config/trainer.ini 的各个段。默认值和 trainer.example.ini 保持一致。
struct Config {
    // [general]
    bool enabled = true;
    int panicKey = VK_F12;
    int menuKey = VK_INSERT;
    int hideKey = VK_F10; // 隐藏/显示覆盖层窗口（功能照常）
    int exitKey = VK_END; // 退出程序
    std::wstring processName = L"Slotbound.exe";

    // [coins] [spins]  写内存功能，锁定默认关
    VarConfig coins{ false, 99999, 1000, 0, 0 };
    VarConfig spins{ false, 99, 5, 0, 0 };
    VarConfig jungsu{ false, 99, 5, 0, 0 }; // 水晶（정수，精华）

    // [slot]  老虎机（经 Lua 桥，需要注入）
    bool slotJackpot = false;        // 大奖模式：每次转都是 9 格同一兵种
    std::string slotSymbol = "A";    // W A S M H D，或 random = 每次随机一种

    // [debug]
    bool overlay = true; // 左上角状态栏

    bool operator==(const Config&) const = default; // 自动保存用：和上次存盘的比
};

// 文件不存在时返回 false，cfg 保持默认值。
bool LoadConfig(const std::wstring& path, Config& cfg);
bool SaveConfig(const std::wstring& path, const Config& cfg);
