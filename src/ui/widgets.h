#pragma once

// 各页共用的界面小件：颜色、开关、按键绑定、等级下拉框、目录分组标题……

#include <string>
#include <vector>

#include "imgui.h"

namespace ui {

inline const ImVec4 kGreen(0.4f, 1.0f, 0.4f, 1.0f);
inline const ImVec4 kRed(1.0f, 0.45f, 0.45f, 1.0f);
inline const ImVec4 kAmber(1.0f, 0.75f, 0.3f, 1.0f);
inline const ImVec4 kText(0.85f, 0.87f, 0.9f, 1.0f);   // 普通条目
inline const ImVec4 kMuted(0.5f, 0.5f, 0.55f, 1.0f);   // 不适用 / 灰掉的条目

// 目录 / 列表表格的统一样式
inline constexpr ImGuiTableFlags kListTable =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
inline constexpr ImGuiTableFlags kSlotTable = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;

// 当前 DPI 缩放（尺寸常量都按 100% 写，乘它）
inline float Scale()
{
    return ImGui::GetStyle().FontScaleDpi;
}

std::string ToUtf8(const std::wstring& s);
std::string KeyName(int vk);

// 拨动开关（代替复选框）：左边文字，右边药丸形状的开关，点文字或开关都能切换。返回 true 表示这一帧切换了
bool ToggleSwitch(const char* label, bool* v, const char* tooltip = nullptr);

// 小节标题：强调色文字 + 细分隔线
void SectionTitle(const char* text);

// 按键绑定：点一下按钮，再按想要的键（Esc 取消）。optional = 旁边多一个「清除」按钮，可以不绑
void KeyBinder(const char* label, int& vk, bool optional = false);

// 等级下拉框（1..maxLevel）：返回 true = 这一帧改了
bool LevelCombo(const char* id, int& level, int maxLevel);

// 下一个控件放得下就接在同一行，放不下就换行（窗口宽度 / DPI 变了也不会超出）
void SameLineIfFits(const char* nextLabel);

// 非空才显示：红色错误 / 灰色提示
void ErrorText(const std::string& text);
void HintText(const std::string& text);

// 第 level 级的效果文字（1 起）；越界返回空串
const std::string& LevelText(const std::vector<std::string>& levels, int level);

// 鼠标停在上一个控件上时，列出每一级的效果；levelNames 为空时显示「N 级」
void LevelsTooltip(const std::vector<std::string>& levels, const char* const* levelNames = nullptr);

// 目录里按稀有度分组的可折叠标题，默认展开；返回 true = 展开着
bool GroupHeader(const char* name, const ImVec4& color, int count, const char* id);

// 槽位表第一列：单选圆点 + 序号（target 选中的槽位，0 = 新增）
void SlotPicker(int& target, int value, int number);

// 「用这个」会作用到哪：替换第几个 / 新增
void TargetSlotHint(int target);

} // namespace ui
