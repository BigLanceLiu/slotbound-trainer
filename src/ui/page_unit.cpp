// 「单位」页：选中单位的星级、吸收 LV、基础属性、技能冷却 / 触发条件；下面分「词条」「刻印」「觉醒」三个子页

#include <cstdio>

#include "bridge/embedded_dll.h"
#include "bridge/injector.h"
#include "labels.h"
#include "pages.h"
#include "widgets.h"

namespace ui {

namespace {

using Unit = UnitLevelEditor::Unit;

// 换了单位：输入框重置成它的当前值
void SyncInputs(UnitInputs& in, const Unit& unit)
{
    const std::string key = UnitKey(unit.sym, unit.rosterIndex);
    if (key == in.unitKey)
        return;
    in.unitKey = key;
    in.level = unit.absorbLv;
    in.stats[0] = unit.hp;
    in.stats[1] = unit.atk;
    in.stats[2] = unit.aspd;
    in.stats[3] = unit.range;
    in.cooldown = unit.SkillCd();
    for (bool& b : in.paramInit)
        b = false;
}

void DrawHeader(const Unit& unit)
{
    const bool hasClass = !unit.subclass.empty();
    ImGui::Text("单位 %s%s%s%s  品级 %s  阵容第 %d 个", unit.sym.c_str(), hasClass ? "（" : "", unit.subclass.c_str(),
        hasClass ? "）" : "", GradeName(unit.grade).c_str(), unit.rosterIndex);
    ImGui::SameLine();
    if (unit.locked)
        ImGui::TextColored(kGreen, "（锁定中）");
    else
        ImGui::TextDisabled("（最近选中的，面板已关）");
    ImGui::TextDisabled("生命 %.0f   攻击 %.1f   攻速 %.2f   射程 %.0f", unit.hp, unit.atk, unit.aspd, unit.range);
}

void DrawRank(UnitLevelEditor& editor, const Unit& unit)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("星级 %d / %d", unit.rank, unit.maxRank);
    ImGui::BeginDisabled(editor.Busy() || unit.needsClassSelect);
    for (int r = unit.rank + 1; r <= unit.maxRank; ++r) {
        const int idx = r - 2;
        const int need = idx >= 0 && idx < static_cast<int>(unit.thresholds.size()) ? unit.thresholds[idx] : 0;
        char label[48];
        snprintf(label, sizeof(label), "升到 %d 星##r%d", r, r);
        ImGui::SameLine();
        if (ImGui::Button(label))
            editor.RaiseRank(r);
        ImGui::SetItemTooltip("走游戏命运石的升星流程：LV 不到 %d 会先补到 %d，然后游戏弹出转职选择，选完职业才真正升星", need, need);
    }
    ImGui::EndDisabled();
    if (unit.rank >= unit.maxRank) {
        ImGui::SameLine();
        ImGui::TextDisabled("已经是这个品级的最高星");
    }
    if (unit.needsClassSelect)
        ImGui::TextColored(kAmber, "等你在游戏里选职业（目标 %d 星）；选完才算升星", unit.pendingRank);
}

void DrawAbsorbLevel(UnitLevelEditor& editor, const Unit& unit, UnitInputs& in)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("吸收 LV %d", unit.absorbLv);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f * Scale());
    ImGui::InputInt("##lv", &in.level, 1, 10);
    if (in.level < 1)
        in.level = 1;
    ImGui::BeginDisabled(editor.Busy());
    ImGui::SameLine();
    if (ImGui::Button("改成这个"))
        editor.SetLevel(in.level);
    for (int add : { 1, 5, 10 }) {
        char label[16];
        snprintf(label, sizeof(label), "+%d", add);
        ImGui::SameLine();
        if (ImGui::Button(label))
            editor.SetLevel(unit.absorbLv + add);
    }
    ImGui::EndDisabled();
    std::string gates;
    for (size_t i = 0; i < unit.thresholds.size(); ++i)
        gates += (i ? "、" : "") + std::to_string(i + 2) + " 星 LV" + std::to_string(unit.thresholds[i]);
    ImGui::TextDisabled("LV 越高属性越高，可升可降（30 级以后成长减半，游戏规则）。升星门槛：%s；单改 LV 不会自动升星",
        gates.empty() ? "—" : gates.c_str());
}

// 属性表一行的开头：第 1 列标签，然后停在第 2 列、设好输入框宽度（后面是两列按钮）
void RowLabel(const char* text)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(140.0f * Scale());
}

void DrawStatRows(UnitLevelEditor& editor, const Unit& unit, UnitInputs& in)
{
    struct StatRow {
        const char* key;
        const char* label;
        double current;
        double minValue;
        const char* format;
    };
    const StatRow rows[] = {
        { "hp", "生命", unit.hp, 1.0, "%.0f" },
        { "atk", "攻击", unit.atk, 0.0, "%.1f" },
        { "aspd", "攻速", unit.aspd, 0.05, "%.2f" },
        { "distance", "射程", unit.range, 1.0, "%.0f" },
    };
    for (int i = 0; i < 4; ++i) {
        const StatRow& row = rows[i];
        double& input = in.stats[i];
        ImGui::PushID(row.key);
        char label[64], current[32];
        snprintf(current, sizeof(current), row.format, row.current);
        snprintf(label, sizeof(label), "%s %s", row.label, current);
        RowLabel(label);
        ImGui::InputDouble("##v", &input, 0.0, 0.0, row.format);
        if (input < row.minValue)
            input = row.minValue;
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(editor.Busy());
        if (ImGui::Button("改成这个"))
            editor.SetStat(row.key, input, row.label);
        ImGui::TableNextColumn();
        if (ImGui::Button("×2")) {
            input = row.current * 2.0;
            editor.SetStat(row.key, input, row.label);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void DrawCooldownRow(UnitLevelEditor& editor, const Unit& unit, UnitInputs& in)
{
    if (!unit.HasActiveCd())
        return;
    constexpr double kMin = UnitLevelEditor::kMinCdSeconds;
    ImGui::PushID("skillcd");
    char label[64];
    snprintf(label, sizeof(label), "技能冷却 %.2f 秒%s", unit.SkillCd(), unit.skillCdCustom ? "*" : "");
    RowLabel(label);
    ImGui::InputDouble("##v", &in.cooldown, 0.0, 0.0, "%.2f");
    if (in.cooldown < kMin)
        in.cooldown = kMin;
    ImGui::TableNextColumn();
    ImGui::BeginDisabled(editor.Busy());
    if (ImGui::Button("改成这个"))
        editor.SetSkillCooldown(in.cooldown);
    ImGui::TableNextColumn();
    if (ImGui::Button("最短")) {
        in.cooldown = kMin;
        editor.SetSkillCooldown(kMin);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!unit.skillCdCustom);
    if (ImGui::Button("原版")) {
        in.cooldown = unit.NaturalCd();
        editor.SetSkillCooldown(-1.0);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::PopID();
}

// 被动技能的触发条件
void DrawParamRows(UnitLevelEditor& editor, const Unit& unit, UnitInputs& in)
{
    struct ParamUi {
        const char* key;
        const char* label;
        const char* unitText;
        double scale;      // 显示值 = 游戏值 × scale（概率显示成 %）
        double minValue;   // 显示值的范围
        double maxValue;
        const char* format;
        bool integer;      // 次数取整
    };
    static const ParamUi kParams[3] = {
        { "charges", "触发所需普攻", "次", 1.0, 1.0, 1000.0, "%.0f", true },
        { "chance", "触发概率", "%", 100.0, 0.0, 100.0, "%.0f", false },
        { "internalCooldown", "内置冷却", "秒", 1.0, 0.0, 600.0, "%.2f", false },
    };
    for (const UnitLevelEditor::Param& p : unit.skillParams) {
        int idx = -1;
        for (int i = 0; i < 3; ++i)
            if (p.key == kParams[i].key)
                idx = i;
        if (idx < 0)
            continue;
        const ParamUi& ui = kParams[idx];
        double& input = in.params[idx];
        if (!in.paramInit[idx]) {
            input = p.current * ui.scale;
            in.paramInit[idx] = true;
        }
        const bool custom = p.current != p.base;
        ImGui::PushID(ui.key);
        char cur[32], label[96];
        snprintf(cur, sizeof(cur), ui.format, p.current * ui.scale);
        snprintf(label, sizeof(label), "%s %s %s%s", ui.label, cur, ui.unitText, custom ? "*" : "");
        RowLabel(label);
        ImGui::InputDouble("##v", &input, 0.0, 0.0, ui.format);
        if (input < ui.minValue)
            input = ui.minValue;
        if (input > ui.maxValue)
            input = ui.maxValue;
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(editor.Busy());
        if (ImGui::Button("改成这个")) {
            double v = input / ui.scale;
            if (ui.integer)
                v = static_cast<double>(static_cast<long long>(v + 0.5));
            editor.SetSkillParam(ui.key, v, false, ui.label);
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(!custom);
        if (ImGui::Button("原版")) {
            input = p.base * ui.scale;
            editor.SetSkillParam(ui.key, 0, true, ui.label);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void DrawSkillNote(const Unit& unit)
{
    using Kind = UnitLevelEditor::SkillKind;
    const char* name = unit.skillName.c_str();
    if (unit.HasActiveCd())
        ImGui::TextDisabled("技能「%s」不改时冷却 %.2f 秒（基础 %.1f 秒 × 技能加速词条）%s。最短 0.1 秒。"
                            "改动存在存档的单位上一直有效，但要开着修改器（已注入）才起作用；不开修改器就是原版冷却",
            name, unit.NaturalCd(), unit.skillBaseCd, unit.skillCdCustom ? "；带 * 表示已自定义" : "");
    else if (unit.skill == Kind::Passive && !unit.skillParams.empty())
        ImGui::TextDisabled("技能「%s」是条件触发的被动技能，上面可以改触发条件（带 * 表示已自定义）。"
                            "改动存在存档的单位上一直有效，要开着修改器（已注入）才起作用；战斗中改当场生效",
            name);
    else if (unit.skill == Kind::Passive)
        ImGui::TextDisabled("技能「%s」是被动技能，触发方式（击杀 / 充能 / 复活）目前改不了", name);
    else
        ImGui::TextDisabled("这个单位目前没有技能（有些技能要到指定星级才解锁）");
}

void DrawSubTabs(LuaPages& pages)
{
    if (!ImGui::BeginTabBar("unitTabs"))
        return;
    if (ImGui::BeginTabItem("词条")) {
        pages.shown.options = true;
        DrawOptionsBody(pages.options, pages.optionsUi);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("刻印")) {
        pages.shown.imprints = true;
        DrawImprintsBody(pages.imprints, pages.imprintsUi);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("觉醒")) {
        pages.shown.awakening = true;
        DrawAwakeningBody(pages.awakening, pages.awakeningUi);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

} // namespace

void FollowTargetSlot(int& target, std::string& unitKey, const std::string& sym, int rosterIndex, int count)
{
    const std::string key = UnitKey(sym, rosterIndex);
    if (key != unitKey) {
        unitKey = key;
        target = 0;
    }
    if (target < 0 || target > count)
        target = 0;
}

bool DrawBridgeBar(const MenuContext& ctx, const char* feature)
{
    std::string& injectError = ctx.pages.injectError;
    if (!ctx.game.Pid()) {
        ImGui::TextDisabled("没连上游戏");
        return false;
    }
    if (!ctx.injected) {
        ImGui::TextWrapped("改%s要在游戏里执行 Lua，需要先把 slotbound_lua.dll 注入游戏。"
                           "它只改 love.dll 导入表里的一个函数指针，不改游戏文件，点「卸载注入」或游戏退出就没了。", feature);
        if (ImGui::Button("注入（启用单位 / 物品 / 老虎机功能）")) {
            injectError.clear();
            // DLL 内嵌在修改器 exe 里，先释放到 %LOCALAPPDATA%\slotbound-trainer\ 再注入
            const std::wstring dll = bridge::ExtractEmbeddedDll(injectError);
            if (dll.empty() || !bridge::Inject(ctx.game.Pid(), dll, injectError))
                injectError = "注入失败：" + injectError;
        }
        ErrorText(injectError);
        return false;
    }
    ImGui::TextColored(kGreen, "Lua 桥已注入");
    ImGui::SameLine();
    if (ImGui::SmallButton("卸载注入"))
        bridge::Unload();
    return true;
}

void DrawUnitPage(MenuContext& ctx)
{
    if (!DrawBridgeBar(ctx, "单位"))
        return;
    LuaPages& pages = ctx.pages;
    pages.shown.unit = true;
    UnitLevelEditor& editor = pages.unit;
    ErrorText(editor.Error());

    const Unit& unit = editor.Selected();
    SectionTitle("选中的单位");
    if (!unit.valid) {
        ImGui::TextColored(kAmber, "请先在游戏里点一下己方单位，这里会显示它的等级、词条和刻印");
        return;
    }
    SyncInputs(pages.unitInputs, unit);
    DrawHeader(unit);
    DrawRank(editor, unit);
    DrawAbsorbLevel(editor, unit, pages.unitInputs);

    // 基础属性：unit.stats，每场开战按它生成战场单位
    SectionTitle("基础属性");
    if (ImGui::BeginTable("stats", 4, ImGuiTableFlags_SizingFixedFit)) {
        DrawStatRows(editor, unit, pages.unitInputs);
        DrawCooldownRow(editor, unit, pages.unitInputs);
        DrawParamRows(editor, unit, pages.unitInputs);
        ImGui::EndTable();
    }
    DrawSkillNote(unit);
    ImGui::TextDisabled("改的是存档里的基础值，一直有效；战斗中改，当前这场也按比例立即生效。词条、刻印、道具的加成照样乘在上面。"
                        "攻速 = 每秒攻击次数");
    HintText(editor.Message());

    ImGui::Spacing();
    DrawSubTabs(pages);
}

} // namespace ui
