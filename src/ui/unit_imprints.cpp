// 「单位」页 →「刻印」子页

#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

using Def = ImprintEditor::Def;

// 刻印最多几级：按定义的等级表；查不到定义时按游戏的 5 级
int MaxLevel(const Def* d)
{
    if (!d)
        return 5;
    return d->levels.empty() ? 1 : static_cast<int>(d->levels.size());
}

void DrawSlotCount(ImprintEditor& editor, const ImprintEditor::Unit& unit)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("刻印槽 %d 个", unit.maxSlots);
    ImGui::SetItemTooltip("游戏面板按这个数画刻印格子，吸收单位得新刻印时也按它判断满没满。\n"
                          "游戏只在单位品级变化时按品级重设（普通 1 / 高级 2 / 稀有、特殊 3 / 传说 4 / 棱彩 5）");
    ImGui::SameLine();
    ImGui::BeginDisabled(editor.Busy());
    if (ImGui::SmallButton(" - ") && unit.maxSlots > 1)
        editor.SetMaxSlots(unit.maxSlots - 1);
    ImGui::SameLine();
    if (ImGui::SmallButton(" + "))
        editor.SetMaxSlots(unit.maxSlots + 1);
    ImGui::EndDisabled();
    if (static_cast<int>(unit.slots.size()) > unit.maxSlots) {
        ImGui::SameLine();
        ImGui::TextColored(kAmber, "刻印比槽多，面板可能显示不全（效果照样生效）");
    }
}

void DrawSlots(ImprintEditor& editor, ImprintsUi& ui, const ImprintEditor::Unit& unit)
{
    const float scale = Scale();
    const int count = static_cast<int>(unit.slots.size());
    if (!ImGui::BeginTable("islots", 5, kSlotTable))
        return;
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 44.0f * scale);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90.0f * scale);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 40.0f * scale);
    for (const ImprintEditor::Slot& s : unit.slots) {
        const Def* d = editor.Find(s.id);
        ImGui::TableNextRow();
        ImGui::PushID(s.index);
        ImGui::TableNextColumn();
        SlotPicker(ui.targetSlot, s.index, s.index);
        ImGui::TableNextColumn();
        ImGui::TextColored(d ? kGreen : kRed, "%s", d ? d->name.c_str() : s.id.c_str());
        ImGui::TableNextColumn();
        int level = s.level;
        ImGui::BeginDisabled(editor.Busy() || !d);
        if (LevelCombo("##lv", level, MaxLevel(d)))
            editor.Set(s.index, s.id, level);
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        if (d)
            ImGui::TextWrapped("%s", LevelText(d->levels, s.level).c_str());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(editor.Busy());
        if (ImGui::SmallButton("删除"))
            editor.Remove(s.index);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID("new");
    SlotPicker(ui.targetSlot, 0, count + 1);
    ImGui::PopID();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("新增（默认）");
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("在下面点「用这个」就加一个；槽不够会自动加");
    ImGui::EndTable();
}

void DrawRow(ImprintEditor& editor, ImprintsUi& ui, const ImprintEditor::Unit& unit, const Def& d, int& level)
{
    int owned = 0;
    for (const ImprintEditor::Slot& s : unit.slots)
        owned += s.id == d.id;
    ImGui::TableNextRow();
    ImGui::PushID(d.id.c_str());
    ImGui::TableNextColumn();
    ImGui::TextColored(owned ? kGreen : kText, "%s", d.name.c_str());
    ImGui::TextDisabled("%s", d.id.c_str());
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", LevelText(d.levels, level).c_str());
    LevelsTooltip(d.levels);
    if (d.id == unit.signature)
        ImGui::TextDisabled("这个单位天生的刻印");
    if (owned > 0)
        ImGui::TextColored(kAmber, "已有 %d 个（重复的效果怎么算没验证过）", owned);
    ImGui::TableNextColumn();
    LevelCombo("##lv", level, MaxLevel(&d));
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(editor.Busy());
    if (ImGui::SmallButton("用这个"))
        editor.Set(ui.targetSlot > 0 ? ui.targetSlot : static_cast<int>(unit.slots.size()) + 1, d.id, level);
    ImGui::EndDisabled();
    ImGui::PopID();
}

} // namespace

void DrawImprintsBody(ImprintEditor& editor, ImprintsUi& ui)
{
    const float scale = Scale();
    if (ImGui::SmallButton("重读刻印表"))
        editor.ReloadCatalog();
    ErrorText(editor.Error());

    const ImprintEditor::Unit& unit = editor.Selected();
    if (!unit.valid) {
        ImGui::TextDisabled("正在读这个单位的刻印…");
        return;
    }
    if (const Def* sig = editor.Find(unit.signature))
        ImGui::TextDisabled("这个兵种 / 品级天生的刻印：%s", sig->name.c_str());

    DrawSlotCount(editor, unit);
    FollowTargetSlot(ui.targetSlot, ui.unitKey, unit.sym, unit.rosterIndex, static_cast<int>(unit.slots.size()));
    DrawSlots(editor, ui, unit);
    ImGui::TextDisabled("等级下拉框直接改等级。默认是新增；要替换某个就先点它左边的圆点。战斗外改最稳");
    HintText(editor.Message());

    SectionTitle("全部刻印");
    ImGui::SetNextItemWidth(200.0f * scale);
    ImGui::InputTextWithHint("##ifilter", "搜索名字 / 效果", ui.filter, sizeof(ui.filter));
    ImGui::SameLine();
    TargetSlotHint(ui.targetSlot);
    if (editor.Catalog().empty()) {
        ImGui::TextDisabled("正在读刻印表…");
        return;
    }

    ImGui::BeginChild("icatalog", ImVec2(0.0f, 380.0f * scale));
    if (ImGui::BeginTable("imprints", 4, kListTable)) {
        ImGui::TableSetupColumn("刻印", ImGuiTableColumnFlags_WidthFixed, 110.0f * scale);
        ImGui::TableSetupColumn("效果", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("等级", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
        for (const Def& d : editor.Catalog()) {
            int& level = ui.levelPick.try_emplace(d.id, MaxLevel(&d)).first->second;
            if (ui.filter[0] && d.name.find(ui.filter) == std::string::npos &&
                LevelText(d.levels, level).find(ui.filter) == std::string::npos)
                continue;
            DrawRow(editor, ui, unit, d, level);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

} // namespace ui
