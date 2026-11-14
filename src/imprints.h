#pragma once

#include <future>
#include <string>
#include <vector>

#include "bridge/luaclient.h"

// 刻印（imprint）编辑：经游戏内的 slotbound_lua.dll 在游戏主线程执行 Lua。
//
// 游戏里的结构：
//   require('src.data.constants').imprint.definitions[id]   定义：name、tag、condition、icon、levels[1..5]{desc, shortDesc, stats}
//   require('src.data.constants').imprint.slots[品级]        该品级的刻印槽数（normal 1 … prism 5）
//   unit.imprints[i]       实例：定义字段摊平 + level、maxLevel、rarity、stats（= levels[level].stats 同一张表）、desc
//   unit.maxImprintSlots   槽数；只在单位品级变化时按 slots 表重设，不会删掉多出来的刻印
//   战斗效果 pairs(unit.imprints) 按 id 匹配、按 level 取数值 → 数量没有上限
class ImprintEditor {
public:
    struct Def {
        std::string id, name, tag;
        std::vector<std::string> levels; // 每级效果（中文），下标 0 = 1 级
    };
    struct Slot {
        int index = 0; // 1 起
        std::string id;
        int level = 1;
    };
    struct Unit {
        bool valid = false;
        std::string sym, grade, signature; // signature = 这个兵种/品级天生的刻印 id
        int rank = 0, rosterIndex = 0, maxSlots = 1;
        bool locked = true;
        std::vector<Slot> slots;
    };

    // 每帧调用：收后台结果；visible 时每 0.5 秒刷新一次选中单位，第一次连上时读刻印目录
    void Update(bool visible);

    // 把 slot（1 起；= 现有数量 + 1 表示新增）设成 id 的 level 级刻印；槽数不够时自动加到够
    void Set(int slot, const std::string& id, int level);
    void Remove(int slot);
    void SetMaxSlots(int slots);
    void ReloadCatalog() { catalog_.clear(); }

    bool Busy() const { return action_.valid(); }
    const std::vector<Def>& Catalog() const { return catalog_; }
    const Def* Find(const std::string& id) const;
    const Unit& Selected() const { return unit_; }
    const std::string& Message() const { return message_; }
    const std::string& Error() const { return error_; }

private:
    void Act(std::string code, std::string message);

    std::future<bridge::Result> catalogJob_;
    std::future<bridge::Result> unitJob_;
    std::future<bridge::Result> action_;
    std::vector<Def> catalog_;
    Unit unit_;
    std::string message_;
    std::string error_;
    unsigned long long lastPoll_ = 0;
};
