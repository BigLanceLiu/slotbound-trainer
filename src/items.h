#pragma once

#include <future>
#include <string>
#include <vector>

#include "bridge/luaclient.h"

// 物品（商店里买的那些，背包在战场下方）编辑：经游戏内的 slotbound_lua.dll 执行 Lua。
//
// 游戏里的结构（反汇编 love.mousepressed 的购买段、restorePurchasedItems，见 docs/findings.md「物品」）：
//   require('src.game.item').getAll() / getById(id)   63 件定义：id、price、rarity C/U/R/L、maxLevel、typeKo、各级数值表
//   ui._itemPurchased[i]       背包：定义的浅拷贝 + level；ui._itemPurchasedKeys[i] = 'p' .. ui._itemPanelNextId（面板动画用的键）
//   买新的：复制、level = 1、updateEffectString、插到末尾；买重复的：level = min(maxLevel, level + 1)
//   getItemInventoryCapacity() = min(8, 6 + 加成)：游戏买新物品时按它判断满没满
//   效果都是战斗中实时查 _itemPurchased（没有 apply 一类的钩子）→ 直接改列表就生效，存档按 id + level 恢复
class ItemEditor {
public:
    struct Def {
        std::string id, name, type, rarity, stage;
        int price = 0, maxLevel = 1;
        bool unlocked = true;            // initiallyUnlocked（没解锁的商店里不出）
        std::vector<std::string> levels; // 每级效果（中文），下标 0 = 1 级
    };
    struct Owned {
        int index = 0;                   // 1 起
        std::string id;
        int level = 1, maxLevel = 1;
    };

    // 每帧调用：visible 时每 0.5 秒刷新背包，第一次连上时读物品目录
    void Update(bool visible);

    // 放进背包：已有同 id 的就把它改成 level 级，没有就新增一件
    void Add(const std::string& id, int level);
    void SetLevel(int index, const std::string& id, int level);
    void Remove(int index, const std::string& id);
    void ReloadCatalog() { catalog_.clear(); }

    bool Busy() const { return action_.valid(); }
    bool Loaded() const { return loaded_; }
    const std::vector<Def>& Catalog() const { return catalog_; }
    const Def* Find(const std::string& id) const;
    const std::vector<Owned>& Inventory() const { return inventory_; }
    int Capacity() const { return capacity_; }
    const std::string& Message() const { return message_; }
    const std::string& Error() const { return error_; }

private:
    void Act(std::string code, std::string message);

    std::future<bridge::Result> catalogJob_;
    std::future<bridge::Result> invJob_;
    std::future<bridge::Result> action_;
    std::vector<Def> catalog_;
    std::vector<Owned> inventory_;
    int capacity_ = 6;
    bool loaded_ = false;
    std::string message_;
    std::string error_;
    unsigned long long lastPoll_ = 0;
};
