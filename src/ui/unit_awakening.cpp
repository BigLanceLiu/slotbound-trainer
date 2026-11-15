// 「单位」页 →「觉醒」子页

#include <algorithm>

#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

using Def = AwakeningEditor::Def;
constexpr int kMax = AwakeningEditor::kMaxSelected;
constexpr const char* kRoman[] = { "", "I", "II", "III", "IV", "V" };

bool IsSelected(const AwakeningEditor::Unit& unit, const std::string& id)
{
    return std::find(unit.selected.begin(), unit.selected.end(), id) != unit.selected.end();
}

void DrawSelected(AwakeningEditor& editor, AwakeningUi& ui, const AwakeningEditor::Unit& unit, int shownLevel)
{
    const float scale = Scale();
    const int count = static_cast<int>(unit.selected.size());
    if (!ImGui::BeginTable("aslots", 4, kSlotTable))
        return;
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 44.0f * scale);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.0f * scale);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 40.0f * scale);
    for (int i = 0; i < count; ++i) {
        const Def* d = editor.Find(unit.selected[i]);
        ImGui::TableNextRow();
        ImGui::PushID(i);
        ImGui::TableNextColumn();
        SlotPicker(ui.targetSlot, i + 1, i + 1);
        ImGui::TableNextColumn();
        ImGui::TextColored(d ? kGreen : kRed, "%s", d ? d->name.c_str() : unit.selected[i].c_str());
        if (d && !d->inPool)
            ImGui::TextColored(kAmber, "不是本职业的");
        ImGui::TableNextColumn();
        if (d)
            ImGui::TextWrapped("%s", LevelText(d->levels, shownLevel).c_str());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(editor.Busy());
        if (ImGui::SmallButton("删除"))
            editor.Remove(i + 1);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if (count < kMax) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID("new");
        SlotPicker(ui.targetSlot, 0, count + 1);
        ImGui::PopID();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("新增（默认）");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("加一个，全部觉醒升到 %s 级", kRoman[count + 1]);
    } else if (ui.targetSlot == 0) {
        ui.targetSlot = 1; // 满 5 个只能替换
    }
    ImGui::EndTable();
}

void DrawCandidates(AwakeningEditor& editor, AwakeningUi& ui, const AwakeningEditor::Unit& unit, int shownLevel)
{
    const float scale = Scale();
    const int count = static_cast<int>(unit.selected.size());
    // 选了以后的等级：新增 = 个数 + 1，替换 = 个数不变
    const int levelAfter = std::min(kMax, ui.targetSlot > 0 ? count : count + 1);
    if (!ImGui::BeginTable("acatalog", 3, kListTable))
        return;
    ImGui::TableSetupColumn("觉醒", ImGuiTableColumnFlags_WidthFixed, 110.0f * scale);
    ImGui::TableSetupColumn("效果（选上以后的等级）", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
    ImGui::TableHeadersRow();
    for (const Def& d : editor.Defs()) {
        if (!d.inPool)
            continue;
        const bool owned = IsSelected(unit, d.id);
        ImGui::TableNextRow();
        ImGui::PushID(d.id.c_str());
        ImGui::TableNextColumn();
        ImGui::TextColored(owned ? kGreen : kText, "%s", d.name.c_str());
        ImGui::TextDisabled("%s", d.id.c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", LevelText(d.levels, owned ? shownLevel : levelAfter).c_str());
        LevelsTooltip(d.levels, kRoman);
        ImGui::TableNextColumn();
        if (owned) {
            ImGui::TextDisabled("已选");
        } else {
            ImGui::BeginDisabled(editor.Busy() || (ui.targetSlot == 0 && count >= kMax));
            if (ImGui::SmallButton("用这个"))
                editor.Set(ui.targetSlot > 0 ? ui.targetSlot : count + 1, d.id);
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

} // namespace

void DrawAwakeningBody(AwakeningEditor& editor, AwakeningUi& ui)
{
    ErrorText(editor.Error());
    const AwakeningEditor::Unit& unit = editor.Selected();
    if (!unit.valid) {
        ImGui::TextDisabled("正在读这个单位的觉醒…");
        return;
    }
    if (!unit.hasAwakening) {
        ImGui::TextDisabled("这个单位没有觉醒（要有职业身份，一般是 3 星选完职业以后）");
        return;
    }
    const int count = static_cast<int>(unit.selected.size());
    const int shownLevel = unit.level >= 0 && unit.level <= kMax ? unit.level : 0;
    ImGui::Text("已选 %d 个，全部是 %s 级", count, kRoman[shownLevel]);
    ImGui::SameLine();
    ImGui::TextDisabled("（游戏容量 %d）", unit.capacity);
    if (unit.version == 10)
        ImGui::TextDisabled("规则：选了几个觉醒，每个就是几级，最多 5 个 = 全部 V 级。效果战斗中实时生效，存档保存，不用开着修改器");

    FollowTargetSlot(ui.targetSlot, ui.unitKey, unit.sym, unit.rosterIndex, count);
    DrawSelected(editor, ui, unit, shownLevel);
    HintText(editor.Message());

    SectionTitle("这个职业的觉醒");
    TargetSlotHint(ui.targetSlot);
    DrawCandidates(editor, ui, unit, shownLevel);
}

} // namespace ui
