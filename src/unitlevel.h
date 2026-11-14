#pragma once

#include <future>
#include <string>
#include <vector>

#include "bridge/luaclient.h"

// 单位等级：星级（rank，游戏叫「转职」）和吸收 LV（absorbLV）。都调游戏自己的函数，属性由游戏重算。
//
//   __slotSetAwakeningLevel(u, lv)   设吸收 LV，并按「已算进属性的 LV(_absorbLvApplied) → 新 LV」补属性差；可升可降
//   __slotStoneRaiseRank(u, 目标星)  命运石升星：LV 不够先补到门槛，再走升星流程 → 游戏弹出转职选择，选完才真正升星；只能往上
//   constants.evolution.rankThresholds[品级][星] = 升到该星需要的 LV；maxRankByGrade[品级] = 最高星级（目前都是 3）
class UnitLevelEditor {
public:
    struct Unit {
        bool valid = false;
        std::string sym, grade, subclass;
        int rank = 1, rosterIndex = 0, maxRank = 1, pendingRank = 0;
        int absorbLv = 1;
        bool locked = true;
        bool needsClassSelect = false;   // 升星后等玩家选职业
        std::vector<int> thresholds;     // [i] = 升到 i+2 星需要的 LV
        double hp = 0, atk = 0, aspd = 0, range = 0;
    };

    // 每帧调用：visible 时每 0.5 秒刷新一次选中单位
    void Update(bool visible);

    void SetLevel(int level);
    void RaiseRank(int rank);

    bool Busy() const { return action_.valid(); }
    const Unit& Selected() const { return unit_; }
    const std::string& Message() const { return message_; }
    const std::string& Error() const { return error_; }

private:
    void Act(std::string code, std::string message);

    std::future<bridge::Result> unitJob_;
    std::future<bridge::Result> action_;
    Unit unit_;
    std::string message_;
    std::string error_;
    unsigned long long lastPoll_ = 0;
};
