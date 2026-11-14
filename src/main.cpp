// 转运军团 修改器：外部程序。读写游戏内存，透明覆盖层画 UI。
// 资源页：外部读写 Lua 全局变量 coins（金币）、spinsLeft（剩余旋转次数）、jungsu（水晶），不注入。
// 词条 / 刻印页：用户点「注入」后经 slotbound_lua.dll 在游戏里执行 Lua，改选中单位的 randomOptions / imprints。

#include <windows.h>
#include <shellapi.h>

#include <cfloat>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "bridge/injector.h"
#include "config.h"
#include "imgui.h"
#include "imprints.h"
#include "luavar.h"
#include "options.h"
#include "overlay.h"
#include "process.h"

#define SB_WIDEN2(x) L##x
#define SB_WIDEN(x) SB_WIDEN2(x)

namespace {

const ImVec4 kGreen(0.4f, 1.0f, 0.4f, 1.0f);
const ImVec4 kRed(1.0f, 0.45f, 0.45f, 1.0f);
const ImVec4 kAmber(1.0f, 0.75f, 0.3f, 1.0f);

// 按键边沿检测：只在按下的那一帧返回 true
class KeyEdge {
public:
    bool Pressed(int vk)
    {
        if (!vk)
            return false;
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool pressed = down && !down_;
        down_ = down;
        return pressed;
    }

private:
    bool down_ = false;
};

std::string ToUtf8(const std::wstring& s)
{
    if (s.empty())
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

const char* KeyName(int vk)
{
    switch (vk) {
    case 0: return "无";
    case VK_LBUTTON: return "鼠标左键";
    case VK_RBUTTON: return "鼠标右键";
    case VK_MBUTTON: return "鼠标中键";
    case VK_XBUTTON1: return "鼠标侧键1";
    case VK_XBUTTON2: return "鼠标侧键2";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_CAPITAL: return "CapsLock";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    }
    static char buf[16];
    if (vk >= VK_F1 && vk <= VK_F24)
        snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
    else if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        snprintf(buf, sizeof(buf), "%c", vk);
    else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        snprintf(buf, sizeof(buf), "小键盘%d", vk - VK_NUMPAD0);
    else
        snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

// 拨动开关（代替复选框）：左边文字，右边药丸形状的开关，点文字或开关都能切换。返回 true 表示这一帧切换了
bool ToggleSwitch(const char* label, bool* v, const char* tooltip = nullptr)
{
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float height = ImGui::GetFrameHeight() * 0.78f;
    const float width = height * 1.9f;
    const float labelWidth = 200.0f * scale;
    ImGui::PushID(label);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float rowHeight = ImGui::GetFrameHeight();
    const bool clicked = ImGui::InvisibleButton("##toggle", ImVec2(labelWidth + width, rowHeight));
    if (clicked)
        *v = !*v;
    const bool hovered = ImGui::IsItemHovered();
    if (tooltip)
        ImGui::SetItemTooltip("%s", tooltip);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec4* colors = ImGui::GetStyle().Colors;
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(start.x, start.y + (rowHeight - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label);

    const ImVec2 p0(start.x + labelWidth, start.y + (rowHeight - height) * 0.5f);
    const ImVec2 p1(p0.x + width, p0.y + height);
    const ImVec4 off = hovered ? colors[ImGuiCol_FrameBgHovered] : colors[ImGuiCol_FrameBg];
    const ImVec4 on = hovered ? colors[ImGuiCol_SliderGrabActive] : colors[ImGuiCol_CheckMark];
    draw->AddRectFilled(p0, p1, ImGui::GetColorU32(*v ? on : off), height * 0.5f);
    const float r = height * 0.5f - 2.0f * scale;
    const float knobX = *v ? p1.x - height * 0.5f : p0.x + height * 0.5f;
    draw->AddCircleFilled(ImVec2(knobX, p0.y + height * 0.5f), r, IM_COL32(245, 247, 250, 255));
    ImGui::PopID();
    return clicked;
}

// 小节标题：强调色文字 + 细分隔线
void SectionTitle(const char* text)
{
    ImGui::Spacing();
    ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], "%s", text);
    ImGui::Separator();
}

// 按键绑定控件：点一下按钮，再按想要的键（Esc 取消）。optional = 旁边多一个"清除"按钮，可以不绑
void KeyBinder(const char* label, int& vk, bool optional = false)
{
    static const char* waitingFor = nullptr;
    static bool waitRelease = false;           // 点按钮的那下左键还没松开，先不收键
    static const char* suppressClick = nullptr; // 用左键绑定后，松开那一下会被按钮当成点击，忽略掉
    const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(200.0f * ImGui::GetStyle().FontScaleDpi);

    if (waitingFor == label) {
        ImGui::Button("请按键…  (Esc 取消)");
        if (waitRelease && !leftDown)
            waitRelease = false;
        for (int key = VK_LBUTTON; key <= 0xFE && !waitRelease; ++key) {
            if (!(GetAsyncKeyState(key) & 0x8000))
                continue;
            if (key != VK_ESCAPE)
                vk = key;
            waitingFor = nullptr;
            suppressClick = label;
            break;
        }
    } else if (ImGui::Button(KeyName(vk)) && suppressClick != label) {
        waitingFor = label;
        waitRelease = true;
    }
    if (suppressClick == label && !leftDown)
        suppressClick = nullptr;
    if (optional && vk && waitingFor != label) {
        ImGui::SameLine();
        if (ImGui::SmallButton("清除"))
            vk = 0;
    }
    ImGui::PopID();
}

// 一个可改数值在界面上的样子
struct VarUi {
    const char* label;    // 「金币」
    int quickAdd[3];      // 快捷按钮 +N
    int step, stepFast;   // 输入框 +/- 按钮的步长
    int setValue;         // 「改成这个数」输入框里的数
    std::string message;  // 上一次操作的结果
};

// 一个可改数值：游戏里的 Lua 全局 + 配置 + 界面 + 快捷键状态
struct Tracked {
    LuaVar var;
    VarConfig Config::*cfg;
    VarUi ui;
    KeyEdge lockEdge;
    KeyEdge addEdge;
};

const char* StateText(const LuaVar& var)
{
    switch (var.GetState()) {
    case LuaVar::State::NotConnected: return "没连上游戏";
    case LuaVar::State::Locating: return "正在找…";
    case LuaVar::State::NotFound: return "没找到（主菜单 / 加载中？5 秒后再找）";
    case LuaVar::State::Ready: return "";
    }
    return "";
}

void DrawStatusVar(const Config& cfg, const VarConfig& vc, const LuaVar& var, const char* label)
{
    if (!var.Ready()) {
        ImGui::TextDisabled("%s：%s", label, StateText(var));
        return;
    }
    ImGui::Text("%s %.0f", label, var.Value());
    if (cfg.enabled && vc.lock) {
        ImGui::SameLine();
        ImGui::TextColored(kGreen, "  锁定在 %d", vc.lockValue);
    }
}

template <size_t N>
void DrawStatus(const Config& cfg, const GameProcess& game, const Tracked (&items)[N])
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
        for (const Tracked& t : items)
            DrawStatusVar(cfg, cfg.*t.cfg, t.var, t.ui.label);
    }

    // KeyName 用静态缓冲区，一次只能拿一个，分开打
    ImGui::TextDisabled("%s 菜单", KeyName(cfg.menuKey));
    ImGui::SameLine();
    ImGui::TextDisabled("%s 总开关", KeyName(cfg.panicKey));
    ImGui::SameLine();
    ImGui::TextDisabled("%s 隐藏", KeyName(cfg.hideKey));
    ImGui::SameLine();
    ImGui::TextDisabled("%s 退出", KeyName(cfg.exitKey));
    ImGui::End();
}

// 「资源」页里一个数值的区块：当前值、改成 / 加、锁定、快捷键
void DrawVarSection(const Config& cfg, VarConfig& vc, LuaVar& var, VarUi& ui)
{
    const float scale = ImGui::GetStyle().FontScaleDpi;
    ImGui::PushID(ui.label);
    SectionTitle(ui.label);
    if (!var.Ready())
        ImGui::TextDisabled("%s", StateText(var));
    else
        ImGui::Text("当前 %.0f", var.Value());

    ImGui::BeginDisabled(!var.Ready() || !cfg.enabled);
    ImGui::SetNextItemWidth(160.0f * scale);
    ImGui::InputInt("##set", &ui.setValue, ui.step, ui.stepFast);
    if (ui.setValue < 0)
        ui.setValue = 0;
    ImGui::SameLine();
    if (ImGui::Button("改成这个数"))
        ui.message = var.Set(ui.setValue) ? "已改" : "写入失败：位置变了，正在重新找";
    for (int i = 0; i < 3; ++i) {
        ImGui::SameLine();
        char label[24];
        snprintf(label, sizeof(label), "+%d", ui.quickAdd[i]);
        if (ImGui::Button(label))
            ui.message = var.Set(var.Value() + ui.quickAdd[i]) ? "已加" : "写入失败：位置变了，正在重新找";
    }
    ImGui::EndDisabled();
    if (!ui.message.empty())
        ImGui::TextDisabled("%s", ui.message.c_str());

    char text[64];
    snprintf(text, sizeof(text), "锁定%s", ui.label);
    ToggleSwitch(text, &vc.lock, "用掉了马上补回锁定值；关掉后从当前值正常减");
    ImGui::SetNextItemWidth(160.0f * scale);
    ImGui::InputInt("锁定值", &vc.lockValue, ui.step, ui.stepFast);
    if (vc.lockValue < 0)
        vc.lockValue = 0;
    KeyBinder("锁定快捷键", vc.lockKey, true);
    ImGui::SetNextItemWidth(160.0f * scale);
    ImGui::InputInt("快捷键每次加", &vc.addAmount, ui.step, ui.stepFast);
    if (vc.addAmount < 0)
        vc.addAmount = 0;
    KeyBinder("加的快捷键", vc.addKey, true);
    ImGui::PopID();
}

// 效果文字里的 {atk.png} 之类是游戏内图标，换成文字
std::string IconText(std::string s)
{
    static const std::pair<const char*, const char*> kIcons[] = {
        { "{atk.png}", "攻击" }, { "{hp.png}", "生命" }, { "{aspd.png}", "攻速" }, { "{dist.png}", "射程" },
        { "{gold.png}", "金币" }, { "{essence.png}", "水晶" },
    };
    for (const auto& [from, to] : kIcons)
        for (size_t pos; (pos = s.find(from)) != std::string::npos;)
            s.replace(pos, strlen(from), to);
    return s;
}

ImVec4 RarityColor(const std::string& rarity)
{
    if (rarity == "legend")
        return ImVec4(1.0f, 0.78f, 0.3f, 1.0f);
    if (rarity == "rare")
        return ImVec4(0.45f, 0.7f, 1.0f, 1.0f);
    return ImVec4(0.85f, 0.87f, 0.9f, 1.0f);
}

const char* RarityName(const std::string& rarity)
{
    if (rarity == "legend")
        return "传说";
    if (rarity == "rare")
        return "稀有";
    return "普通";
}

const char* GradeName(const std::string& grade)
{
    static const std::pair<const char*, const char*> kGrades[] = {
        { "normal", "普通" }, { "advanced", "高级" }, { "rare", "稀有" },
        { "special", "特殊" }, { "legendary", "传说" }, { "prism", "棱彩" },
    };
    for (const auto& [key, name] : kGrades)
        if (grade == key)
            return name;
    return grade.c_str();
}

// 词条 / 刻印页的目标槽位：0 = 新增（默认）。换了单位、或选中的槽被删掉了，都回到新增
void FollowTargetSlot(int& target, std::string& unitKey, const std::string& sym, int rosterIndex, int count)
{
    const std::string key = sym + "#" + std::to_string(rosterIndex);
    if (key != unitKey) {
        unitKey = key;
        target = 0;
    }
    if (target < 0 || target > count)
        target = 0;
}

void TargetSlotHint(int target)
{
    if (target > 0)
        ImGui::TextColored(kAmber, "点「用这个」会替换第 %d 个", target);
    else
        ImGui::TextDisabled("点「用这个」会新增一个");
}

// 「词条」页的界面状态
struct OptionsUi {
    int targetSlot = 0;                      // 改到第几个槽位（1 起）；0 = 新增（默认）
    std::string unitKey;                     // 换了单位就回到「新增」
    std::map<std::string, int> variantPick;  // 每个词条选的档位下标，-1 = 随机
    char filter[64] = {};
    bool onlyFit = false;
    std::string injectError;
};

// 词条页、刻印页顶部：没注入时给注入按钮；已注入时显示状态和「卸载注入」。返回 true = 可以用
bool DrawBridgeBar(const GameProcess& game, std::string& injectError, const char* feature)
{
    if (!game.Pid()) {
        ImGui::TextDisabled("没连上游戏");
        return false;
    }
    if (!bridge::Injected()) {
        ImGui::TextWrapped("改%s要在游戏里执行 Lua，需要先把 slotbound_lua.dll 注入游戏。"
                           "它只改 love.dll 导入表里的一个函数指针，不改游戏文件，点「卸载注入」或游戏退出就没了。", feature);
        if (ImGui::Button("注入（启用词条 / 刻印功能）")) {
            injectError.clear();
            if (!bridge::Inject(game.Pid(), L"", injectError))
                injectError = "注入失败：" + injectError;
        }
        if (!injectError.empty())
            ImGui::TextColored(kRed, "%s", injectError.c_str());
        return false;
    }
    ImGui::TextColored(kGreen, "Lua 桥已注入");
    ImGui::SameLine();
    if (ImGui::SmallButton("卸载注入"))
        bridge::Unload();
    return true;
}

void DrawOptionsTab(const GameProcess& game, OptionEditor& editor, OptionsUi& ui)
{
    const float scale = ImGui::GetStyle().FontScaleDpi;
    if (!DrawBridgeBar(game, ui.injectError, "词条"))
        return;
    ImGui::SameLine();
    if (ImGui::SmallButton("重读词条表"))
        editor.ReloadCatalog();
    if (!editor.Error().empty())
        ImGui::TextColored(kRed, "%s", editor.Error().c_str());

    const OptionEditor::Unit& unit = editor.Selected();
    SectionTitle("选中的单位");
    if (!unit.valid) {
        ImGui::TextColored(kAmber, "请先在游戏里点一下己方单位，这里会显示它的词条");
        return;
    }
    ImGui::Text("单位 %s  品级 %s  等级 %d  阵容第 %d 个", unit.sym.c_str(), GradeName(unit.grade), unit.rank, unit.rosterIndex);
    ImGui::SameLine();
    if (unit.locked)
        ImGui::TextColored(kGreen, "（锁定中）");
    else
        ImGui::TextDisabled("（最近选中的，面板已关）");

    const int count = static_cast<int>(unit.slots.size());
    FollowTargetSlot(ui.targetSlot, ui.unitKey, unit.sym, unit.rosterIndex, count);
    if (ImGui::BeginTable("slots", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        for (const OptionEditor::Slot& s : unit.slots) {
            const OptionEditor::Def* d = editor.Find(s.id);
            ImGui::TableNextRow();
            ImGui::PushID(s.index);
            ImGui::TableNextColumn();
            ImGui::RadioButton("##slot", &ui.targetSlot, s.index);
            ImGui::SameLine();
            ImGui::Text("%d", s.index);
            ImGui::TableNextColumn();
            ImGui::TextColored(d ? RarityColor(d->rarity) : kRed, "%s %s", d ? d->name.c_str() : s.id.c_str(), s.value.c_str());
            ImGui::TableNextColumn();
            if (d)
                ImGui::TextWrapped("%s", IconText(d->effect).c_str());
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(editor.Busy());
            if (ImGui::SmallButton("删除"))
                editor.RemoveOption(s.index);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        // 游戏里没有词条数量上限：randomOptions 是普通列表，结算全用 ipairs 逐个算；
        // 只有一次抽词条的 RO.rollList(n) 会把 n 截到 5
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::RadioButton("##new", &ui.targetSlot, 0);
            ImGui::SameLine();
            ImGui::Text("%d", count + 1);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("（新增，默认：在下面点「用这个」就加一个）");
            if (count >= 5) {
                ImGui::TableNextColumn();
                ImGui::TextColored(kAmber, "已超过游戏一次最多抽的 5 个，面板可能显示不全");
            }
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("默认是新增；要替换某个就先点它左边的圆点。战斗中改的话下一场战斗才算属性");
    if (!editor.Message().empty())
        ImGui::TextDisabled("%s", editor.Message().c_str());

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
    for (const char* rarity : { "legend", "rare", "common" }) {
        int total = 0;
        for (const OptionEditor::Def& d : editor.Catalog())
            total += d.rarity == rarity;
        char header[64];
        snprintf(header, sizeof(header), "%s（%d）###%s", RarityName(rarity), total, rarity);
        ImGui::PushStyleColor(ImGuiCol_Text, RarityColor(rarity));
        const bool open = ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor();
        if (!open)
            continue;
        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
        if (!ImGui::BeginTable(rarity, 4, flags))
            continue;
        ImGui::TableSetupColumn("词条", ImGuiTableColumnFlags_WidthFixed, 120.0f * scale);
        ImGui::TableSetupColumn("效果", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthFixed, 110.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
        for (const OptionEditor::Def& d : editor.Catalog()) {
            if (d.rarity != rarity)
                continue;
            const std::string effect = IconText(d.effect);
            if (ui.filter[0] && d.name.find(ui.filter) == std::string::npos && effect.find(ui.filter) == std::string::npos)
                continue;
            const auto fitIt = unit.fit.find(d.id);
            const std::string reason = fitIt != unit.fit.end() ? fitIt->second.reason : std::string();
            const int owned = fitIt != unit.fit.end() ? fitIt->second.owned : 0;
            if (ui.onlyFit && !reason.empty())
                continue;
            ImGui::TableNextRow();
            ImGui::PushID(d.id.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(reason.empty() ? RarityColor(d.rarity) : ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "%s", d.name.c_str());
            ImGui::TextDisabled("%s", d.id.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", effect.c_str());
            if (!d.notes.empty())
                ImGui::TextDisabled("%s", d.notes.c_str());
            if (!reason.empty())
                ImGui::TextColored(kRed, "这个单位抽不到：%s", reason.c_str());
            if (owned > 0)
                ImGui::TextColored(kAmber, "已有 %d 个，再加会叠加（效果相乘）", owned);
            ImGui::TableNextColumn();
            int& pick = ui.variantPick.try_emplace(d.id, d.variants.empty() ? -1 : static_cast<int>(d.variants.size()) - 1).first->second;
            if (!d.variants.empty()) {
                char preview[48];
                if (pick < 0)
                    snprintf(preview, sizeof(preview), "随机");
                else
                    snprintf(preview, sizeof(preview), "%s", d.variants[pick].value.c_str());
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##v", preview)) {
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
            } else {
                ImGui::TextDisabled("固定");
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(editor.Busy());
            if (ImGui::SmallButton(reason.empty() ? "用这个" : "强加"))
                editor.SetOption(ui.targetSlot > 0 ? ui.targetSlot : count + 1, d.id, pick >= 0 && pick < static_cast<int>(d.variants.size()) ? d.variants[pick].value : "");
            if (!reason.empty())
                ImGui::SetItemTooltip("游戏规则下这个单位抽不到；强加后可能没效果或表现异常");
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

// 「刻印」页的界面状态
struct ImprintsUi {
    int targetSlot = 0;                    // 改到第几个槽位（1 起）；0 = 新增（默认）
    std::string unitKey;                   // 换了单位就回到「新增」
    std::map<std::string, int> levelPick;  // 目录里每个刻印选的等级（1 起）
    char filter[64] = {};
    std::string injectError;
};

// 等级下拉框：返回 true = 这一帧改了
bool LevelCombo(const char* id, int& level, int maxLevel)
{
    char preview[16];
    snprintf(preview, sizeof(preview), "%d 级", level);
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(id, preview)) {
        for (int i = 1; i <= maxLevel; ++i) {
            char label[16];
            snprintf(label, sizeof(label), "%d 级", i);
            if (ImGui::Selectable(label, level == i) && level != i) {
                level = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

// 第 level 级的效果文字（1 起）
std::string ImprintEffect(const ImprintEditor::Def* d, int level)
{
    if (!d || level < 1 || level > static_cast<int>(d->levels.size()))
        return {};
    return IconText(d->levels[level - 1]);
}

void DrawImprintsTab(const GameProcess& game, ImprintEditor& editor, ImprintsUi& ui)
{
    const float scale = ImGui::GetStyle().FontScaleDpi;
    if (!DrawBridgeBar(game, ui.injectError, "刻印"))
        return;
    ImGui::SameLine();
    if (ImGui::SmallButton("重读刻印表"))
        editor.ReloadCatalog();
    if (!editor.Error().empty())
        ImGui::TextColored(kRed, "%s", editor.Error().c_str());

    const ImprintEditor::Unit& unit = editor.Selected();
    SectionTitle("选中的单位");
    if (!unit.valid) {
        ImGui::TextColored(kAmber, "请先在游戏里点一下己方单位，这里会显示它的刻印");
        return;
    }
    ImGui::Text("单位 %s  品级 %s  等级 %d  阵容第 %d 个", unit.sym.c_str(), GradeName(unit.grade), unit.rank, unit.rosterIndex);
    ImGui::SameLine();
    if (unit.locked)
        ImGui::TextColored(kGreen, "（锁定中）");
    else
        ImGui::TextDisabled("（最近选中的，面板已关）");
    if (const ImprintEditor::Def* sig = editor.Find(unit.signature))
        ImGui::TextDisabled("这个兵种 / 品级天生的刻印：%s", sig->name.c_str());

    const int count = static_cast<int>(unit.slots.size());
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
    if (count > unit.maxSlots) {
        ImGui::SameLine();
        ImGui::TextColored(kAmber, "刻印比槽多，面板可能显示不全（效果照样生效）");
    }

    FollowTargetSlot(ui.targetSlot, ui.unitKey, unit.sym, unit.rosterIndex, count);
    if (ImGui::BeginTable("islots", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 44.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 40.0f * scale);
        for (const ImprintEditor::Slot& s : unit.slots) {
            const ImprintEditor::Def* d = editor.Find(s.id);
            ImGui::TableNextRow();
            ImGui::PushID(s.index);
            ImGui::TableNextColumn();
            ImGui::RadioButton("##slot", &ui.targetSlot, s.index);
            ImGui::SameLine();
            ImGui::Text("%d", s.index);
            ImGui::TableNextColumn();
            ImGui::TextColored(d ? kGreen : kRed, "%s", d ? d->name.c_str() : s.id.c_str());
            ImGui::TableNextColumn();
            int level = s.level;
            ImGui::BeginDisabled(editor.Busy() || !d);
            if (LevelCombo("##lv", level, d ? static_cast<int>(d->levels.size()) : 5))
                editor.Set(s.index, s.id, level);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", ImprintEffect(d, s.level).c_str());
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(editor.Busy());
            if (ImGui::SmallButton("删除"))
                editor.Remove(s.index);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::RadioButton("##new", &ui.targetSlot, 0);
        ImGui::SameLine();
        ImGui::Text("%d", count + 1);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("新增（默认）");
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("在下面点「用这个」就加一个；槽不够会自动加");
        ImGui::EndTable();
    }
    ImGui::TextDisabled("等级下拉框直接改等级。默认是新增；要替换某个就先点它左边的圆点。战斗外改最稳");
    if (!editor.Message().empty())
        ImGui::TextDisabled("%s", editor.Message().c_str());

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
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("imprints", 4, flags)) {
        ImGui::TableSetupColumn("刻印", ImGuiTableColumnFlags_WidthFixed, 110.0f * scale);
        ImGui::TableSetupColumn("效果", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("等级", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
        for (const ImprintEditor::Def& d : editor.Catalog()) {
            const int maxLevel = static_cast<int>(d.levels.size());
            int& level = ui.levelPick.try_emplace(d.id, maxLevel > 0 ? maxLevel : 1).first->second;
            const std::string effect = ImprintEffect(&d, level);
            if (ui.filter[0] && d.name.find(ui.filter) == std::string::npos && effect.find(ui.filter) == std::string::npos)
                continue;
            int owned = 0;
            for (const ImprintEditor::Slot& s : unit.slots)
                owned += s.id == d.id;
            ImGui::TableNextRow();
            ImGui::PushID(d.id.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(owned ? kGreen : ImVec4(0.85f, 0.87f, 0.9f, 1.0f), "%s", d.name.c_str());
            ImGui::TextDisabled("%s", d.id.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", effect.c_str());
            if (ImGui::IsItemHovered() && maxLevel > 0) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(420.0f * scale);
                for (int i = 1; i <= maxLevel; ++i)
                    ImGui::Text("%d 级：%s", i, ImprintEffect(&d, i).c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            if (d.id == unit.signature)
                ImGui::TextDisabled("这个单位天生的刻印");
            if (owned > 0)
                ImGui::TextColored(kAmber, "已有 %d 个（重复的效果怎么算没验证过）", owned);
            ImGui::TableNextColumn();
            LevelCombo("##lv", level, maxLevel > 0 ? maxLevel : 1);
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(editor.Busy());
            if (ImGui::SmallButton("用这个"))
                editor.Set(ui.targetSlot > 0 ? ui.targetSlot : count + 1, d.id, level);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

// 经 Lua 桥工作的几页（词条、刻印）：编辑器 + 界面状态 + 这一帧哪页开着
struct LuaTabs {
    OptionEditor options;
    OptionsUi optionsUi;
    bool optionsOpen = false;
    ImprintEditor imprints;
    ImprintsUi imprintsUi;
    bool imprintsOpen = false;
};

// 返回 false 表示用户点了「退出」
template <size_t N>
bool DrawMenu(Config& cfg, const std::wstring& cfgPath, bool& cfgFileFound, const GameProcess& game, Tracked (&items)[N],
    LuaTabs& tabs)
{
    bool keepRunning = true;
    const float scale = ImGui::GetStyle().FontScaleDpi;
    tabs.optionsOpen = false;
    tabs.imprintsOpen = false;
    ImGui::SetNextWindowSize(ImVec2(640.0f * scale, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(80.0f * scale, 200.0f * scale), ImGuiCond_FirstUseEver);
    ImGui::Begin("转运军团 修改器", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ToggleSwitch("总开关", &cfg.enabled, "急停：停止所有锁定，修改按钮也不能用");

    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("资源")) {
            for (Tracked& t : items)
                DrawVarSection(cfg, cfg.*t.cfg, t.var, t.ui);
            ImGui::Spacing();
            if (!cfg.enabled)
                ImGui::TextColored(kAmber, "总开关是关的：修改和锁定都不生效");
            ImGui::TextDisabled("改出来的数值不会在退出修改器时恢复（本来就是想留着的）");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("词条")) {
            tabs.optionsOpen = true;
            DrawOptionsTab(game, tabs.options, tabs.optionsUi);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("刻印")) {
            tabs.imprintsOpen = true;
            DrawImprintsTab(game, tabs.imprints, tabs.imprintsUi);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("诊断")) {
            if (game.Pid())
                ImGui::Text("进程 %s  pid %lu  写权限%s", ToUtf8(cfg.processName).c_str(), game.Pid(),
                    game.CanWrite() ? "已开" : "未开");
            else
                ImGui::TextDisabled("没连上游戏");
            bool locating = false;
            for (const Tracked& t : items) {
                SectionTitle(t.var.Name());
                if (t.var.Ready())
                    ImGui::Text("节点 0x%llX", static_cast<unsigned long long>(t.var.Node()));
                else
                    ImGui::TextDisabled("%s", StateText(t.var));
                if (!t.var.Log().empty())
                    ImGui::TextWrapped("%s", t.var.Log().c_str());
                locating |= t.var.GetState() == LuaVar::State::Locating;
            }
            ImGui::BeginDisabled(!game.Pid() || locating);
            if (ImGui::Button("重新定位")) {
                for (Tracked& t : items)
                    t.var.Relocate();
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("重新扫一遍游戏内存，约一秒；正常不用点，位置变了会自动重找");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("设置")) {
            KeyBinder("菜单键", cfg.menuKey);
            KeyBinder("总开关键", cfg.panicKey);
            KeyBinder("隐藏覆盖层键", cfg.hideKey);
            KeyBinder("退出键", cfg.exitKey);
            ToggleSwitch("左上角状态栏", &cfg.overlay);
            ImGui::TextDisabled("进程名：%s（在 ini 里改）", ToUtf8(cfg.processName).c_str());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s%s", ToUtf8(cfgPath).c_str(), cfgFileFound ? "" : "（不存在，用默认值）");
    ImGui::TextDisabled("改动会自动保存");
    if (ImGui::Button("重新读取"))
        cfgFileFound = LoadConfig(cfgPath, cfg);
    ImGui::SameLine();
    if (ImGui::Button("退出程序"))
        keepRunning = false;

    ImGui::End();
    return keepRunning;
}

// 便携版：exe 旁边的 trainer.ini；开发版：仓库里的 config/trainer.ini
std::wstring DefaultConfigPath()
{
#ifdef SB_PORTABLE
    wchar_t exe[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring path(exe, len);
    path.resize(path.find_last_of(L'\\') + 1);
    return path + L"trainer.ini";
#else
    return SB_WIDEN(SB_DEFAULT_CONFIG);
#endif
}

// 第一个命令行参数是配置文件路径（按 Windows 规则拆参数，去掉引号）
std::wstring ConfigPathFromCommandLine()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring path = argv && argc > 1 ? argv[1] : DefaultConfigPath();
    LocalFree(argv);
    return path;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const std::wstring cfgPath = ConfigPathFromCommandLine();
    Config cfg;
    bool cfgFileFound = LoadConfig(cfgPath, cfg);

    if (!overlay::Create()) {
        MessageBoxW(nullptr, L"覆盖层创建失败（D3D11 初始化不了）", L"转运军团 修改器", MB_ICONERROR);
        return 1;
    }
    overlay::SetInteractive(false);

    GameProcess game;
    // 可改的 Lua 全局变量。displayCoins 只有 _G 里有；别的表（状态快照）里也有同名字段，靠它挑出 _G 的那个。
    // 加新数值：这里加一行 + Config 里加一个 VarConfig + config.cpp 里 Load/Save 一行
    Tracked items[] = {
        { { game, "coins", "displayCoins" }, &Config::coins, { "金币", { 100, 1000, 10000 }, 1000, 10000, 99999, {} } },
        { { game, "spinsLeft", "displayCoins" }, &Config::spins, { "旋转次数", { 1, 5, 10 }, 1, 10, 99, {} } },
        // 水晶的界面动画目标 jungsuBalanceTarget 只在游戏自己加减时更新，改值时要一起写，界面才会跟上
        { { game, "jungsu", "displayCoins", "jungsuBalanceTarget", "jungsuBalance" }, &Config::jungsu,
            { "水晶", { 1, 5, 10 }, 1, 10, 99, {} } },
    };
    LuaTabs tabs;
    bool menuOpen = false;
    KeyEdge menuKey;
    KeyEdge panicKey;
    KeyEdge hideKey;
    KeyEdge exitKey;
    ULONGLONG lastPoll = 0;
    // 自动保存：设置停止变化 0.5 秒后写盘（拖滑块时不会每帧都写）
    Config savedCfg = cfg;
    Config lastSeenCfg = cfg;
    ULONGLONG cfgChangedAt = 0;

    while (overlay::PumpMessages()) {
        const ULONGLONG now = GetTickCount64();
        if (cfg != lastSeenCfg) {
            lastSeenCfg = cfg;
            cfgChangedAt = now;
        }
        if (cfg != savedCfg && now - cfgChangedAt >= 500) {
            cfgFileFound = SaveConfig(cfgPath, cfg);
            savedCfg = cfg;
        }

        // 每秒一次：连接/断开游戏进程、跟随游戏窗口
        if (now - lastPoll >= 1000) {
            lastPoll = now;
            if (game.Pid() && !game.IsAlive()) {
                for (Tracked& t : items)
                    t.var.Reset(); // 先等后台定位线程停下，再关句柄
                game.Detach();
            }
            if (!game.Pid())
                game.Attach(cfg.processName);
            game.RefreshWindow();
            overlay::FollowWindow(game.Window());
        }

        if (menuKey.Pressed(cfg.menuKey)) {
            menuOpen = !menuOpen;
            overlay::SetInteractive(menuOpen);
            if (!menuOpen && game.Window())
                overlay::BringToFront(game.Window());
        }
        if (panicKey.Pressed(cfg.panicKey))
            cfg.enabled = !cfg.enabled;
        if (hideKey.Pressed(cfg.hideKey))
            overlay::SetVisible(!overlay::Visible());
        if (exitKey.Pressed(cfg.exitKey))
            PostQuitMessage(0);
        for (Tracked& t : items) {
            VarConfig& vc = cfg.*t.cfg;
            if (t.lockEdge.Pressed(vc.lockKey))
                vc.lock = !vc.lock;
            if (t.addEdge.Pressed(vc.addKey) && cfg.enabled && t.var.Ready())
                t.var.Set(t.var.Value() + vc.addAmount);
        }
        if (menuOpen)
            overlay::SetVisible(true); // 菜单开着时一定要看得见

        overlay::KeepAbove(game.Window());

        overlay::WaitForVBlank();
        for (Tracked& t : items) {
            const VarConfig& vc = cfg.*t.cfg;
            t.var.Update(cfg.enabled && vc.lock, vc.lockValue);
        }
        const bool luaReady = menuOpen && game.Pid() && bridge::Injected();
        tabs.options.Update(luaReady && tabs.optionsOpen);
        tabs.imprints.Update(luaReady && tabs.imprintsOpen);
        overlay::BeginFrame();

        if (cfg.overlay)
            DrawStatus(cfg, game, items);
        if (menuOpen && !DrawMenu(cfg, cfgPath, cfgFileFound, game, items, tabs))
            PostQuitMessage(0);

        overlay::EndFrame();
    }

    for (Tracked& t : items)
        t.var.Reset();
    if (cfg != savedCfg)
        SaveConfig(cfgPath, cfg);
    overlay::Destroy();
    return 0;
}
