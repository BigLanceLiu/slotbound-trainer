#pragma once

#include <future>
#include <string>
#include <vector>

#include "bridge/luaclient.h"

// 觉醒（awakening）编辑：经游戏内的 slotbound_lua.dll 执行 Lua。
//
// 游戏里的结构（require('src.game.awakening') = A，反汇编见 docs/findings.md「觉醒」）：
//   unit.awakening = { version = 10, unitId, selected = { 'W12', 'W28', ... }, levels, stage, seen, pending }
//   A.pool(u)         这个职业的候选觉醒（12 个，A.data.units[identity].candidates）
//   A.level(u, id)    第 10 版：选中的话 = min(5, #selected) → 选几个，全部就是几级
//   A.has(u, id)      selected 里有、且在总目录里 → 战斗中各效果实时查，改了马上生效
//   A.normalize(u)    去重、只留总目录里有的、最多 5 个，重设 stage / levels（不看容量，也不看是不是本职业的）
class AwakeningEditor {
public:
    struct Def {
        std::string id, name, category;
        std::vector<std::string> levels; // 每级效果（中文），下标 0 = 1 级
        bool inPool = false;             // 是这个职业的候选
    };
    struct Unit {
        bool valid = false;
        bool hasAwakening = false;       // 这个单位有觉醒（有职业身份）
        std::string sym, identity;
        int rosterIndex = 0, version = 10, capacity = 0, level = 0;
        std::vector<std::string> selected;
        std::vector<Def> defs;           // 候选池 + 已选里不在池中的
    };

    static constexpr int kMaxSelected = 5;

    // 每帧调用：visible 时每 0.5 秒刷新一次选中单位
    void Update(bool visible);

    // 把第 slot 个（1 起；= 现有个数 + 1 表示新增）设成 id
    void Set(int slot, const std::string& id);
    void Remove(int slot);

    bool Busy() const { return action_.valid(); }
    const Unit& Selected() const { return unit_; }
    const Def* Find(const std::string& id) const;
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
