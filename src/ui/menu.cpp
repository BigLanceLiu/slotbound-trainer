// 主菜单窗口：标签页切换 + 底部的配置文件栏；以及 Lua 桥各页的刷新调度

#include "menu.h"

#include "pages.h"
#include "widgets.h"

namespace ui {

void LuaPages::Update(bool menuOpen, bool injected, const Config& cfg)
{
    // 菜单关着就不去游戏里读（省游戏主线程的时间）；老虎机例外，见下
    const bool ready = menuOpen && injected;
    unit.Update(ready && shown.unit);
    options.Update(ready && shown.unit && shown.options);
    imprints.Update(ready && shown.unit && shown.imprints);
    awakening.Update(ready && shown.unit && shown.awakening);
    items.Update(ready && shown.items);
    // 老虎机控制不管菜单开没开都要保持（每秒检查包装还在不在）
    slot.Update(injected, cfg.enabled && cfg.slotJackpot, cfg.slotSymbol);
}

bool DrawMenu(MenuContext& ctx)
{
    bool keepRunning = true;
    const float scale = Scale();
    ctx.pages.shown = {};
    ImGui::SetNextWindowSize(ImVec2(640.0f * scale, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(80.0f * scale, 200.0f * scale), ImGuiCond_FirstUseEver);
    ImGui::Begin("转运军团 修改器", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ToggleSwitch("总开关", &ctx.cfg.enabled, "急停：停止所有锁定，修改按钮也不能用");

    struct Tab {
        const char* name;
        void (*draw)(MenuContext&);
    };
    static const Tab kTabs[] = {
        { "资源", [](MenuContext& c) { DrawResourcesPage(c.cfg, c.vars); } },
        { "单位", DrawUnitPage },
        { "物品", DrawItemsPage },
        { "老虎机", DrawSlotPage },
        { "诊断", DrawDiagnosticsPage },
        { "设置", DrawSettingsPage },
    };
    if (ImGui::BeginTabBar("tabs")) {
        for (const Tab& tab : kTabs) {
            if (ImGui::BeginTabItem(tab.name)) {
                tab.draw(ctx);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s%s", ToUtf8(ctx.cfgPath).c_str(), ctx.cfgFileFound ? "" : "（不存在，用默认值）");
    ImGui::TextDisabled("改动会自动保存");
    if (ImGui::Button("重新读取"))
        ctx.cfgFileFound = LoadConfig(ctx.cfgPath, ctx.cfg);
    ImGui::SameLine();
    if (ImGui::Button("退出程序"))
        keepRunning = false;

    ImGui::End();
    return keepRunning;
}

} // namespace ui
