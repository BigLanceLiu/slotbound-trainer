#pragma once

#include <future>
#include <string>

#include "bridge/luaclient.h"

// 老虎机控制：经 Lua 桥给 slot.buildResultGrid 包一层。
//
// 游戏的转法：拉杆时 startSpin 先调 slot:buildResultGrid() 把这次的 3×3（grid[行][列]，符号 W/A/S/M/H/D、J 王牌）
// 按权重算好，再让三个转轮滚到这个结果停下 → 改它的返回值，画面和结算都跟着变。
// 包装只读 _G.__sbSlotCfg，关掉时原样透传；新开一局游戏可能换掉 slot 对象，所以每秒检查一次、不在就重新包。
class SlotControl {
public:
    // 每帧调用。injected = Lua 桥可用；jackpot / symbol 是当前设置（总开关关时传 false）
    void Update(bool injected, bool jackpot, const std::string& symbol);

    bool Hooked() const { return hooked_; }
    int Hits() const { return hits_; }          // 本次游戏里大奖模式改过几次结果
    const std::string& Grid() const { return grid_; } // 当前画面上的 3×3，9 个字母按行
    const std::string& Error() const { return error_; }

private:
    std::future<bridge::Result> job_;
    unsigned long long lastSync_ = 0;
    bool sentJackpot_ = false;
    std::string sentSymbol_; // 空 = 还没同步过（第一次一定发）
    bool hooked_ = false;
    int hits_ = 0;
    std::string grid_;
    std::string error_;
};
