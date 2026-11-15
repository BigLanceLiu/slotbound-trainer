// 「诊断」「设置」两页

#include "pages.h"
#include "widgets.h"

namespace ui {

void DrawDiagnosticsPage(MenuContext& ctx)
{
    const GameProcess& game = ctx.game;
    if (game.Pid())
        ImGui::Text("进程 %s  pid %lu  写权限%s", ToUtf8(ctx.cfg.processName).c_str(), game.Pid(),
            game.CanWrite() ? "已开" : "未开");
    else
        ImGui::TextDisabled("没连上游戏");
    ImGui::Text("Lua 桥：%s", ctx.injected ? "已注入" : "未注入");

    bool locating = false;
    for (const TrackedVar& t : ctx.vars) {
        SectionTitle(t.var.Name());
        if (t.var.Ready())
            ImGui::Text("节点 0x%llX", static_cast<unsigned long long>(t.var.Node()));
        else
            ImGui::TextDisabled("%s", VarStateText(t.var));
        if (!t.var.Log().empty())
            ImGui::TextWrapped("%s", t.var.Log().c_str());
        locating |= t.var.GetState() == LuaVar::State::Locating;
    }
    ImGui::BeginDisabled(!game.Pid() || locating);
    if (ImGui::Button("重新定位"))
        for (TrackedVar& t : ctx.vars)
            t.var.Relocate();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("重新扫一遍游戏内存，约一秒；正常不用点，位置变了会自动重找");
}

void DrawSettingsPage(MenuContext& ctx)
{
    Config& cfg = ctx.cfg;
    KeyBinder("菜单键", cfg.menuKey);
    KeyBinder("总开关键", cfg.panicKey);
    KeyBinder("隐藏覆盖层键", cfg.hideKey);
    KeyBinder("退出键", cfg.exitKey);
    ToggleSwitch("左上角状态栏", &cfg.overlay);
    ImGui::TextDisabled("进程名：%s（在 ini 里改）", ToUtf8(cfg.processName).c_str());
}

} // namespace ui
