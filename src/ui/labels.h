#pragma once

// 游戏里的代号 → 界面上的中文和颜色（品级、稀有度、老虎机符号）

#include <string>

#include "imgui.h"

namespace ui {

struct RarityStyle {
    const char* name;
    ImVec4 color;
};

// 单位品级：normal / advanced / rare / special / legendary / prism；不认识的原样返回
std::string GradeName(const std::string& grade);

// 词条稀有度：legend / rare / common
RarityStyle OptionRarity(const std::string& rarity);
inline constexpr const char* kOptionRarities[] = { "legend", "rare", "common" };

// 物品品阶：L 传说 / R 稀有 / U 罕见 / C 普通
RarityStyle ItemRarity(const std::string& rarity);
inline constexpr const char* kItemRarities[] = { "L", "R", "U", "C" };

// 老虎机符号字母 → 兵种名
const char* SlotSymbolName(char c);

} // namespace ui
