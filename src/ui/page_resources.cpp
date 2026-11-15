// 「资源」页（金币 / 旋转次数 / 水晶：外部读写，不注入）和左上角状态栏

#include <cstdio>

#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

constexpr const char* kWriteFailed = "写入失败：位置变了，正在重新找";

void DrawStatusVar(const Config& cfg, const VarConfig& vc, const LuaVar& var, const char* label)
{
    if (!var.Ready()) {
        ImGui::TextDisabled("%s：%s", label, VarStateText(var));
        return;
    }
    ImGui::Text("%s %.0f", label, var.Value());
    if (cfg.enabled && vc.lock) {
        ImGui::SameLine();
        ImGui::TextColored(kGreen, "  锁定在 %d", vc.lockValue);
    }
}

// 输入框：不许负数
void NonNegativeInput(const char* label, int& value, const VarUi& ui)
{
    ImGui::SetNextItemWidth(160.0f * Scale());
    ImGui::InputInt(label, &value, ui.step, ui.stepFast);
    if (value < 0)
        value = 0;
}

// 一个数值的区块：当前值、改成 / 加、锁定、快捷键
void DrawVarSection(const Config& cfg, VarConfig& vc, LuaVar& var, VarUi& ui)
{
    ImGui::PushID(ui.label);
    SectionTitle(ui.label);
    if (!var.Ready())
        ImGui::TextDisabled("%s", VarStateText(var));
    else
        ImGui::Text("当前 %.0f", var.Value());

    ImGui::BeginDisabled(!var.Ready() || !cfg.enabled);
    NonNegativeInput("##set", ui.setValue, ui);
    ImGui::SameLine();
    if (ImGui::Button("改成这个数"))
        ui.message = var.Set(ui.setValue) ? "已改" : kWriteFailed;
    for (int add : ui.quickAdd) {
        ImGui::SameLine();
        char label[24];
        snprintf(label, sizeof(label), "+%d", add);
        if (ImGui::Button(label))
            ui.message = var.Set(var.Value() + add) ? "已加" : kWriteFailed;
    }
    ImGui::EndDisabled();
    HintText(ui.message);

    char text[64];
    snprintf(text, sizeof(text), "锁定%s", ui.label);
    ToggleSwitch(text, &vc.lock, "用掉了马上补回锁定值；关掉后从当前值正常减");
    NonNegativeInput("锁定值", vc.lockValue, ui);
    KeyBinder("锁定快捷键", vc.lockKey, true);
    NonNegativeInput("快捷键每次加", vc.addAmount, ui);
    KeyBinder("加的快捷键", vc.addKey, true);
    ImGui::PopID();
}

} // namespace

const char* VarStateText(const LuaVar& var)
{
    switch (var.GetState()) {
    case LuaVar::State::NotConnected: return "没连上游戏";
    case LuaVar::State::Locating: return "正在找…";
    case LuaVar::State::NotFound: return "没找到（主菜单 / 加载中？5 秒后再找）";
    case LuaVar::State::Ready: return "";
    }
    return "";
}

void DrawStatusBar(const Config& cfg, const GameProcess& game, std::span<const TrackedVar> vars)
{
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("##status", nullptr, flags);

    ImGui::TextUnformatted("转运军团 修改器");
    ImGui::SameLine();
    ImGui::TextColored(cfg.enabled ? kGreen : kRed, cfg.enabled ? "[开]" : "[关]");

    if (!game.Pid()) {
        ImGui::TextDisabled("等待 %s ...", ToUtf8(cfg.processName).c_str());
    } else {
        for (const TrackedVar& t : vars)
            DrawStatusVar(cfg, cfg.*t.cfg, t.var, t.ui.label);
    }

    ImGui::TextDisabled("%s 菜单  %s 总开关  %s 隐藏  %s 退出", KeyName(cfg.menuKey).c_str(), KeyName(cfg.panicKey).c_str(),
        KeyName(cfg.hideKey).c_str(), KeyName(cfg.exitKey).c_str());
    ImGui::End();
}

void DrawResourcesPage(Config& cfg, std::span<TrackedVar> vars)
{
    for (TrackedVar& t : vars)
        DrawVarSection(cfg, cfg.*t.cfg, t.var, t.ui);
    ImGui::Spacing();
    if (!cfg.enabled)
        ImGui::TextColored(kAmber, "总开关是关的：修改和锁定都不生效");
    ImGui::TextDisabled("改出来的数值不会在退出修改器时恢复（本来就是想留着的）");
}

} // namespace ui
