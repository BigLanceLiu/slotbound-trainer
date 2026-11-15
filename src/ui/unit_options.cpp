// 「单位」页 →「词条」子页

#include <cfloat>
#include <cstdio>

#include "labels.h"
#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

using Def = OptionEditor::Def;

void DrawSlots(OptionEditor& editor, OptionsUi& ui, const OptionEditor::Unit& unit)
{
    const int count = static_cast<int>(unit.slots.size());
    if (!ImGui::BeginTable("slots", 4, kSlotTable))
        return;
    for (const OptionEditor::Slot& s : unit.slots) {
        const Def* d = editor.Find(s.id);
        ImGui::TableNextRow();
        ImGui::PushID(s.index);
        ImGui::TableNextColumn();
        SlotPicker(ui.targetSlot, s.index, s.index);
        ImGui::TableNextColumn();
        ImGui::TextColored(d ? OptionRarity(d->rarity).color : kRed, "%s %s", d ? d->name.c_str() : s.id.c_str(),
            s.value.c_str());
        ImGui::TableNextColumn();
        if (d)
            ImGui::TextWrapped("%s", d->effect.c_str());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(editor.Busy());
        if (ImGui::SmallButton("删除"))
            editor.RemoveOption(s.index);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    // 游戏里没有词条数量上限：randomOptions 是普通列表，结算全用 ipairs 逐个算；
    // 只有一次抽词条的 RO.rollList(n) 会把 n 截到 5
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID("new");
    SlotPicker(ui.targetSlot, 0, count + 1);
    ImGui::PopID();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("（新增，默认：在下面点「用这个」就加一个）");
    if (count >= 5) {
        ImGui::TableNextColumn();
        ImGui::TextColored(kAmber, "已超过游戏一次最多抽的 5 个，面板可能显示不全");
    }
    ImGui::EndTable();
}

// 档位下拉框；pick = -1 表示随机
void VariantCombo(const Def& d, int& pick)
{
    if (d.variants.empty()) {
        ImGui::TextDisabled("固定");
        return;
    }
    if (pick >= static_cast<int>(d.variants.size())) // 重读词条表后档位变少了
        pick = static_cast<int>(d.variants.size()) - 1;
    char preview[48];
    snprintf(preview, sizeof(preview), "%s", pick < 0 ? "随机" : d.variants[pick].value.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (!ImGui::BeginCombo("##v", preview))
        return;
    if (ImGui::Selectable("随机（按游戏概率）", pick < 0))
        pick = -1;
    for (int i = 0; i < static_cast<int>(d.variants.size()); ++i) {
        char label[64];
        snprintf(label, sizeof(label), "%s   概率 %.0f%%", d.variants[i].value.c_str(), d.variants[i].chance * 100.0);
        if (ImGui::Selectable(label, pick == i))
            pick = i;
    }
    ImGui::EndCombo();
}

void DrawRow(OptionEditor& editor, OptionsUi& ui, const OptionEditor::Unit& unit, const Def& d, const std::string& reason,
    int owned)
{
    const int count = static_cast<int>(unit.slots.size());
    ImGui::TableNextRow();
    ImGui::PushID(d.id.c_str());
    ImGui::TableNextColumn();
    ImGui::TextColored(reason.empty() ? OptionRarity(d.rarity).color : kMuted, "%s", d.name.c_str());
    ImGui::TextDisabled("%s", d.id.c_str());
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", d.effect.c_str());
    HintText(d.notes);
    if (!reason.empty())
        ImGui::TextColored(kRed, "这个单位抽不到：%s", reason.c_str());
    if (owned > 0)
        ImGui::TextColored(kAmber, "已有 %d 个，再加会叠加（效果相乘）", owned);
    ImGui::TableNextColumn();
    const int last = d.variants.empty() ? -1 : static_cast<int>(d.variants.size()) - 1;
    int& pick = ui.variantPick.try_emplace(d.id, last).first->second;
    VariantCombo(d, pick);
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(editor.Busy());
    if (ImGui::SmallButton(reason.empty() ? "用这个" : "强加")) {
        const bool valid = pick >= 0 && pick < static_cast<int>(d.variants.size());
        editor.SetOption(ui.targetSlot > 0 ? ui.targetSlot : count + 1, d.id, valid ? d.variants[pick].value : "");
    }
    if (!reason.empty())
        ImGui::SetItemTooltip("游戏规则下这个单位抽不到；强加后可能没效果或表现异常");
    ImGui::EndDisabled();
    ImGui::PopID();
}

} // namespace

void DrawOptionsBody(OptionEditor& editor, OptionsUi& ui)
{
    const float scale = Scale();
    if (ImGui::SmallButton("重读词条表"))
        editor.ReloadCatalog();
    ErrorText(editor.Error());

    const OptionEditor::Unit& unit = editor.Selected();
    if (!unit.valid) {
        ImGui::TextDisabled("正在读这个单位的词条…");
        return;
    }
    FollowTargetSlot(ui.targetSlot, ui.unitKey, unit.sym, unit.rosterIndex, static_cast<int>(unit.slots.size()));
    DrawSlots(editor, ui, unit);
    ImGui::TextDisabled("默认是新增；要替换某个就先点它左边的圆点。战斗中改的话下一场战斗才算属性");
    HintText(editor.Message());

    SectionTitle("全部词条");
    ImGui::SetNextItemWidth(200.0f * scale);
    ImGui::InputTextWithHint("##filter", "搜索名字 / 效果", ui.filter, sizeof(ui.filter));
    ImGui::SameLine();
    TargetSlotHint(ui.targetSlot);
    if (editor.Catalog().empty()) {
        ImGui::TextDisabled("正在读词条表…");
        return;
    }
    ImGui::SameLine();
    ImGui::Checkbox("只看适用的", &ui.onlyFit);
    ImGui::SetItemTooltip("隐藏这个单位按游戏规则抽不到的词条（兵种不符、品级不符、互斥等）");

    // 按稀有度分组：传说 → 稀有 → 普通
    ImGui::BeginChild("catalog", ImVec2(0.0f, 380.0f * scale));
    for (const char* rarity : kOptionRarities) {
        int total = 0;
        for (const Def& d : editor.Catalog())
            total += d.rarity == rarity;
        const RarityStyle style = OptionRarity(rarity);
        if (!GroupHeader(style.name, style.color, total, rarity))
            continue;
        if (!ImGui::BeginTable(rarity, 4, kListTable))
            continue;
        ImGui::TableSetupColumn("词条", ImGuiTableColumnFlags_WidthFixed, 120.0f * scale);
        ImGui::TableSetupColumn("效果", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthFixed, 110.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
        for (const Def& d : editor.Catalog()) {
            if (d.rarity != rarity)
                continue;
            if (ui.filter[0] && d.name.find(ui.filter) == std::string::npos && d.effect.find(ui.filter) == std::string::npos)
                continue;
            const auto fit = unit.fit.find(d.id);
            static const OptionEditor::Fit kNoInfo;
            const OptionEditor::Fit& f = fit != unit.fit.end() ? fit->second : kNoInfo;
            if (ui.onlyFit && !f.reason.empty())
                continue;
            DrawRow(editor, ui, unit, d, f.reason, f.owned);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

} // namespace ui
