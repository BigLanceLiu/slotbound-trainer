#pragma once

#include <windows.h>

#include "imgui.h"

// 透明、置顶、鼠标穿透的覆盖窗口，用 D3D11 + Dear ImGui 画。
// 游戏要开「无边框窗口」模式，独占全屏会把覆盖层盖掉。
namespace overlay {

bool Create();
void Destroy();

// 处理窗口消息；收到 WM_QUIT 返回 false
bool PumpMessages();

// 对齐到游戏窗口的客户区；target 为空时铺满主显示器（没开游戏也能调 UI）。位置没变不碰 Z 序
void FollowWindow(HWND target);

// 每帧调用：游戏窗口跑到我们上面了才重新置顶
void KeepAbove(HWND target);

// 隐藏/显示覆盖层窗口（程序其余逻辑照常跑：各项功能不受影响）
void SetVisible(bool visible);
bool Visible();

// 打开菜单时要接收鼠标键盘：去掉穿透并抢前台；关菜单时恢复穿透
void SetInteractive(bool interactive);

// SetForegroundWindow 在后台进程里经常不生效，借前台线程的输入队列绕过去
void BringToFront(HWND hwnd);

ImVec2 Size();

// 每帧最先调用：等到垂直同步再读游戏数据，降低覆盖层相对游戏画面的延迟
void WaitForVBlank();

void BeginFrame();
void EndFrame();

} // namespace overlay
