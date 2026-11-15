// 「物品」页：背包（改等级 / 删除）+ 全部物品（按品阶分组，直接放进背包）

#include <cstdio>

#include "labels.h"
#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

using Def = ItemEditor::Def;

void DrawInventory(ItemEditor& editor)
{
    const float scale = Scale();
    const auto& inv = editor.Inventory();
    SectionTitle("背包");
    ImGui::Text("%d 件 / %d 格", static_cast<int>(inv.size()), editor.Capacity());
    if (static_cast<int>(inv.size()) > editor.Capacity()) {
        ImGui::SameLine();
        ImGui::TextColored(kAmber, "比格子多：界面可能显示不全，商店也买不了新物品（买重复的升级照常）");
    }
    if (ImGui::BeginTable("inv", 4, kSlotTable)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 40.0f * scale);
        for (const ItemEditor::Owned& o : inv) {
            const Def* d = editor.Find(o.id);
            ImGui::TableNextRow();
            ImGui::PushID(o.index);
            ImGui::TableNextColumn();
            ImGui::TextColored(d ? ItemRarity(d->rarity).color : kRed, "%s", d ? d->name.c_str() : o.id.c_str());
            ImGui::TableNextColumn();
            int level = o.level;
            ImGui::BeginDisabled(editor.Busy());
            if (LevelCombo("##lv", level, o.maxLevel))
                editor.SetLevel(o.index, o.id, level);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            if (d)
                ImGui::TextWrapped("%s", LevelText(d->levels, o.level).c_str());
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(editor.Busy());
            if (ImGui::SmallButton("删除"))
                editor.Remove(o.index, o.id);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (inv.empty())
        ImGui::TextDisabled("背包是空的");
    HintText(editor.Message());
    ImGui::TextDisabled("等级最高到该物品的满级（游戏的数值表只有这么多级）。删除不退水晶。效果战斗中实时生效，存档会保存");
}

void DrawCatalogRow(ItemEditor& editor, const Def& d, int& level)
{
    const int ownedLevel = editor.OwnedLevel(d.id);
    const RarityStyle style = ItemRarity(d.rarity);
    ImGui::TableNextRow();
    ImGui::PushID(d.id.c_str());
    ImGui::TableNextColumn();
    ImGui::TextColored(style.color, "%s", d.name.c_str());
    ImGui::TextDisabled("%s · %s · %d 水晶", d.type.c_str(), style.name, d.price);
    if (!d.unlocked)
        ImGui::TextColored(kAmber, "升级树没解锁");
    if (!d.stage.empty())
        ImGui::TextDisabled("第 %s 关的物品", d.stage.c_str());
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", LevelText(d.levels, level).c_str());
    if (d.levels.size() > 1)
        LevelsTooltip(d.levels);
    if (ownedLevel > 0)
        ImGui::TextColored(kGreen, "背包里有，%d 级", ownedLevel);
    ImGui::TableNextColumn();
    LevelCombo("##lv", level, d.maxLevel);
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(editor.Busy() || ownedLevel == level);
    if (ImGui::SmallButton(ownedLevel > 0 ? "改成这级" : "放进背包"))
        editor.Add(d.id, level);
    ImGui::EndDisabled();
    ImGui::PopID();
}

void DrawCatalog(ItemEditor& editor, ItemsUi& ui)
{
    const float scale = Scale();
    SectionTitle("全部物品");
    ImGui::SetNextItemWidth(200.0f * scale);
    ImGui::InputTextWithHint("##filter", "搜索名字 / 效果", ui.filter, sizeof(ui.filter));
    ImGui::SameLine();
    ImGui::TextDisabled("已有的会直接改成选的等级；没解锁、别的关卡的也能放");
    if (editor.Catalog().empty()) {
        ImGui::TextDisabled("正在读物品表…");
        return;
    }

    // 每件物品当前选的等级（默认满级）；搜索按这一级的效果文字匹配
    auto pickedLevel = [&](const Def& d) -> int& { return ui.levelPick.try_emplace(d.id, d.maxLevel).first->second; };
    auto matches = [&](const Def& d) {
        return !ui.filter[0] || d.name.find(ui.filter) != std::string::npos ||
               LevelText(d.levels, pickedLevel(d)).find(ui.filter) != std::string::npos;
    };

    // 按品阶分组：传说 → 稀有 → 罕见 → 普通；搜索时只列有匹配的组，括号里是匹配数
    ImGui::BeginChild("icat", ImVec2(0.0f, 380.0f * scale));
    for (const char* rarity : kItemRarities) {
        int total = 0;
        for (const Def& d : editor.Catalog())
            total += d.rarity == rarity && matches(d);
        if (total == 0)
            continue;
        const RarityStyle style = ItemRarity(rarity);
        char id[16];
        snprintf(id, sizeof(id), "item_%s", rarity);
        if (!GroupHeader(style.name, style.color, total, id))
            continue;
        if (!ImGui::BeginTable(rarity, 4, kListTable))
            continue;
        ImGui::TableSetupColumn("物品", ImGuiTableColumnFlags_WidthFixed, 120.0f * scale);
        ImGui::TableSetupColumn("效果", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("等级", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 72.0f * scale);
        for (const Def& d : editor.Catalog())
            if (d.rarity == rarity && matches(d))
                DrawCatalogRow(editor, d, pickedLevel(d));
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

} // namespace

void DrawItemsPage(MenuContext& ctx)
{
    if (!DrawBridgeBar(ctx, "物品"))
        return;
    ctx.pages.shown.items = true;
    ItemEditor& editor = ctx.pages.items;
    ErrorText(editor.Error());
    if (!editor.Loaded()) {
        ImGui::TextDisabled("正在读背包…");
        return;
    }
    DrawInventory(editor);
    DrawCatalog(editor, ctx.pages.itemsUi);
}

} // namespace ui
