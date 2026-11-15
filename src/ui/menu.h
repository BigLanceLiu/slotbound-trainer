#pragma once

// 菜单和状态栏的对外接口：主循环只认这里的几个类型和函数，各页怎么画见 pages.h

#include <map>
#include <span>
#include <string>

#include "app/config.h"
#include "app/hotkeys.h"
#include "core/luavar.h"
#include "core/process.h"
#include "game/awakening.h"
#include "game/imprints.h"
#include "game/items.h"
#include "game/options.h"
#include "game/slotctl.h"
#include "game/unitlevel.h"

namespace ui {

// 「资源」页里一个数值的界面设定和状态
struct VarUi {
    const char* label;    // 「金币」
    int quickAdd[3];      // 快捷按钮 +N
    int step, stepFast;   // 输入框 +/- 按钮的步长
    int setValue;         // 「改成这个数」输入框里的数
    std::string message;  // 上一次操作的结果
};

// 一个外部读写的数值：游戏里的 Lua 全局 + 配置 + 界面 + 快捷键状态
struct TrackedVar {
    LuaVar var;
    VarConfig Config::*cfg;
    VarUi ui;
    KeyEdge lockEdge;
    KeyEdge addEdge;
};

// 「词条」子页
struct OptionsUi {
    int targetSlot = 0;                      // 改到第几个槽位（1 起）；0 = 新增（默认）
    std::string unitKey;                     // 换了单位就回到「新增」
    std::map<std::string, int> variantPick;  // 每个词条选的档位下标，-1 = 随机
    char filter[64] = {};
    bool onlyFit = false;
};

// 「刻印」子页
struct ImprintsUi {
    int targetSlot = 0;
    std::string unitKey;
    std::map<std::string, int> levelPick;  // 目录里每个刻印选的等级（1 起）
    char filter[64] = {};
};

// 「觉醒」子页
struct AwakeningUi {
    int targetSlot = 0;
    std::string unitKey;
};

// 「单位」页上半部分的输入框；换了单位就重置成它的当前值
struct UnitInputs {
    std::string unitKey;
    int level = 1;
    double stats[4] = {};    // 生命 / 攻击 / 攻速 / 射程
    double cooldown = 0;     // 技能冷却（秒）
    double params[3] = {};   // 被动触发条件：普攻次数 / 概率(%) / 内置冷却
    bool paramInit[3] = {};
};

// 「物品」页
struct ItemsUi {
    std::map<std::string, int> levelPick;  // 目录里每件物品选的等级
    char filter[64] = {};
};

// 经 Lua 桥工作的几页：编辑器 + 界面状态
struct LuaPages {
    std::string injectError;
    SlotControl slot;
    UnitLevelEditor unit;
    OptionEditor options;
    ImprintEditor imprints;
    AwakeningEditor awakening;
    ItemEditor items;

    UnitInputs unitInputs;
    OptionsUi optionsUi;
    ImprintsUi imprintsUi;
    AwakeningUi awakeningUi;
    ItemsUi itemsUi;

    // 这一帧画了哪几页（画菜单前清零）：只给看得见的页刷新数据
    struct Shown {
        bool unit = false, options = false, imprints = false, awakening = false, items = false;
    } shown;

    // 每帧画菜单之前调用。menuOpen / injected 决定要不要去游戏里读数据
    void Update(bool menuOpen, bool injected, const Config& cfg);
};

// 画菜单要用到的东西
struct MenuContext {
    Config& cfg;
    const std::wstring& cfgPath;
    bool& cfgFileFound;
    const GameProcess& game;
    bool injected;                 // Lua 桥已注入（本帧查一次）
    std::span<TrackedVar> vars;
    LuaPages& pages;
};

// 左上角状态栏
void DrawStatusBar(const Config& cfg, const GameProcess& game, std::span<const TrackedVar> vars);

// 主菜单；返回 false 表示用户点了「退出程序」
bool DrawMenu(MenuContext& ctx);

} // namespace ui
