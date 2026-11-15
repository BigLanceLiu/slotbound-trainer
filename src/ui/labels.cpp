#include "labels.h"

#include <utility>

#include "widgets.h"

namespace ui {

std::string GradeName(const std::string& grade)
{
    static const std::pair<const char*, const char*> kGrades[] = {
        { "normal", "普通" }, { "advanced", "高级" }, { "rare", "稀有" },
        { "special", "特殊" }, { "legendary", "传说" }, { "prism", "棱彩" },
    };
    for (const auto& [key, name] : kGrades)
        if (grade == key)
            return name;
    return grade;
}

RarityStyle OptionRarity(const std::string& rarity)
{
    if (rarity == "legend")
        return { "传说", ImVec4(1.0f, 0.78f, 0.3f, 1.0f) };
    if (rarity == "rare")
        return { "稀有", ImVec4(0.45f, 0.7f, 1.0f, 1.0f) };
    return { "普通", kText };
}

RarityStyle ItemRarity(const std::string& rarity)
{
    if (rarity == "L")
        return { "传说", ImVec4(1.0f, 0.7f, 0.3f, 1.0f) };
    if (rarity == "R")
        return { "稀有", ImVec4(0.45f, 0.7f, 1.0f, 1.0f) };
    if (rarity == "U")
        return { "罕见", ImVec4(0.45f, 0.85f, 0.45f, 1.0f) };
    if (rarity == "C")
        return { "普通", kText };
    return { "?", kText };
}

const char* SlotSymbolName(char c)
{
    switch (c) {
    case 'W': return "战士";
    case 'A': return "弓手";
    case 'S': return "枪兵";
    case 'M': return "法师";
    case 'H': return "骑兵";
    case 'D': return "决斗者";
    case 'J': return "小丑";
    case 'B': return "空";
    }
    return "?";
}

} // namespace ui
