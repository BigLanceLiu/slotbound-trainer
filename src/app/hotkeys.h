#pragma once

#include <windows.h>

// 全局快捷键的边沿检测：只在按下的那一帧返回 true（游戏在前台时也能收到）
class KeyEdge {
public:
    bool Pressed(int vk)
    {
        if (!vk)
            return false;
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool pressed = down && !down_;
        down_ = down;
        return pressed;
    }

private:
    bool down_ = false;
};
