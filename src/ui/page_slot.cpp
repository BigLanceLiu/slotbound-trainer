// 「老虎机」页：大奖模式

#include "labels.h"
#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

void DrawSymbolPicker(Config& cfg)
{
    ImGui::TextDisabled("兵种：");
    // J = 小丑：万能符号；整条线都是小丑时出「小丑」单位，9 格全小丑 = 8 条小丑线 + 大奖
    static const char* kSymbols[] = { "W", "A", "S", "M", "H", "D", "J", "random" };
    for (const char* s : kSymbols) {
        const bool random = s[1] != '\0';
        const char* label = random ? "每次随机" : SlotSymbolName(s[0]);
        SameLineIfFits(label);
        if (ImGui::RadioButton(label, cfg.slotSymbol == s))
            cfg.slotSymbol = s;
        if (s[0] == 'J')
            ImGui::SetItemTooltip("小丑是万能符号；整条线都是小丑时出「小丑」单位。9 格全小丑 = 8 条小丑线，也算大奖");
        if (random)
            ImGui::SetItemTooltip("每次从 6 个普通兵种里随机一种（不含小丑）");
    }
}

void DrawStatus(const Config& cfg, const SlotControl& slot)
{
    if (!slot.Hooked())
        ImGui::TextDisabled("正在连接老虎机…（还在主菜单的话，进一局就好）");
    else if (!cfg.enabled)
        ImGui::TextColored(kAmber, "总开关是关的：大奖模式不生效");
    else if (cfg.slotJackpot)
        ImGui::TextColored(kGreen, "已生效：下一次拉杆就是大奖（本局已改 %d 次）", slot.Hits());
    else
        ImGui::TextDisabled("关闭中：转出来的和游戏原本的一样");
    ImGui::TextDisabled("拉杆那一刻定结果，正在转的那一次不受影响。命运石的转动也会被改成大奖");
}

void DrawGrid(const std::string& g)
{
    if (g.size() != 9 || !ImGui::BeginTable("grid", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TextDisabled("—");
        return;
    }
    for (int r = 0; r < 3; ++r) {
        ImGui::TableNextRow();
        for (int c = 0; c < 3; ++c) {
            ImGui::TableNextColumn();
            ImGui::Dummy(ImVec2(70.0f * Scale(), 0.0f));
            ImGui::TextUnformatted(SlotSymbolName(g[r * 3 + c]));
        }
    }
    ImGui::EndTable();
}

} // namespace

void DrawSlotPage(MenuContext& ctx)
{
    if (!DrawBridgeBar(ctx, "老虎机"))
        return;
    const SlotControl& slot = ctx.pages.slot;
    ErrorText(slot.Error());

    SectionTitle("大奖模式");
    ToggleSwitch("每次都转出大奖", &ctx.cfg.slotJackpot, "打开后每次拉杆都是 9 格同一个兵种（游戏的大奖）；关掉恢复正常");
    DrawSymbolPicker(ctx.cfg);
    DrawStatus(ctx.cfg, slot);

    SectionTitle("当前结果");
    DrawGrid(slot.Grid());
}

} // namespace ui
