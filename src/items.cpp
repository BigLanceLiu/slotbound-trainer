#include "items.h"

#include <windows.h>

#include <cstdlib>

#include "luaops.h"

namespace {

using namespace luaops;

// 物品目录：id  名字  类型  稀有度  价格  满级  是否初始解锁  关卡  各级效果（| 分隔）
constexpr const char* kCatalogLua = R"lua(
local I = require('src.game.item')
local Lang = require('src.data.language')
local loc = Lang[rawget(_G, 'currentGameLang') or 'zh'] or Lang.zh or {}
local FS = '\31'
local function clean(s) return (tostring(s or ''):gsub('[\n|\31]', ' ')) end
local types = { ['전투'] = '战斗', ['훈련'] = '训练' }
local out = {}
for _, d in ipairs(I.getAll()) do
  local id = tostring(d.id)
  local maxL = math.max(1, math.floor(tonumber(d.maxLevel) or 1))
  local lv = {}
  for L = 1, maxL do lv[#lv+1] = clean(loc['item.effect.' .. id .. '.' .. L] or (L == 1 and d.effect) or '') end
  out[#out+1] = table.concat({ id, clean(loc['item.name.' .. id] or d.name or id), types[d.typeKo] or tostring(d.typeKo or ''),
    tostring(d.rarity or ''), tostring(d.price or 0), tostring(maxL), d.initiallyUnlocked == false and '0' or '1',
    tostring(d.stage or ''), table.concat(lv, '|') }, FS)
end
return table.concat(out, '\n')
)lua";

// 背包：第一行 容量；之后每行 序号  id  等级  满级
constexpr const char* kInventoryLua = R"lua(
if type(ui) ~= 'table' then return 'noui' end
local FS = '\31'
local cap = type(getItemInventoryCapacity) == 'function' and getItemInventoryCapacity() or 6
local out = { tostring(cap) }
for i, o in ipairs(ui._itemPurchased or {}) do
  out[#out+1] = table.concat({ tostring(i), tostring(o.id), tostring(o.level or 1), tostring(o.maxLevel or 1) }, FS)
end
return table.concat(out, '\n')
)lua";

// 核对背包第 index 件还是 id（防止点按钮前游戏里卖掉 / 买了别的）
std::string CheckSlot(int index, const std::string& id)
{
    return "local list = type(ui) == 'table' and ui._itemPurchased or {}\n"
           "local o = list[" + std::to_string(index) + "]\n"
           "if not o or tostring(o.id) ~= " + Quote(id) + " then return 'changed' end\n";
}

} // namespace

const ItemEditor::Def* ItemEditor::Find(const std::string& id) const
{
    for (const Def& d : catalog_)
        if (d.id == id)
            return &d;
    return nullptr;
}

void ItemEditor::Update(bool visible)
{
    if (Ready(catalogJob_)) {
        const bridge::Result r = catalogJob_.get();
        if (!r.ok) {
            error_ = "读物品目录失败：" + r.text;
        } else {
            catalog_.clear();
            for (const std::string& line : Split(r.text, '\n')) {
                const auto f = Split(line, kFs);
                if (f.size() < 9)
                    continue;
                Def d;
                d.id = f[0];
                d.name = f[1];
                d.type = f[2];
                d.rarity = f[3];
                d.price = atoi(f[4].c_str());
                d.maxLevel = atoi(f[5].c_str());
                d.unlocked = f[6] == "1";
                d.stage = f[7];
                d.levels = Split(f[8], '|');
                catalog_.push_back(std::move(d));
            }
            error_.clear();
        }
    }
    if (Ready(invJob_)) {
        const bridge::Result r = invJob_.get();
        if (r.ok && r.text != "noui") {
            const auto lines = Split(r.text, '\n');
            capacity_ = atoi(lines[0].c_str());
            inventory_.clear();
            for (size_t i = 1; i < lines.size(); ++i) {
                const auto f = Split(lines[i], kFs);
                if (f.size() >= 4)
                    inventory_.push_back({ atoi(f[0].c_str()), f[1], atoi(f[2].c_str()), atoi(f[3].c_str()) });
            }
            loaded_ = true;
            error_.clear();
        } else if (!r.ok) {
            error_ = r.text;
        }
    }
    if (Ready(action_)) {
        const bridge::Result r = action_.get();
        if (!r.ok)
            message_ = "失败：" + r.text;
        else if (r.text == "changed")
            message_ = "失败：背包变了（游戏里刚买 / 卖了东西），请看一眼再点";
        else if (r.text != "ok")
            message_ = "失败：" + r.text;
        lastPoll_ = 0;
    }

    if (!visible)
        return;
    if (catalog_.empty() && !catalogJob_.valid())
        catalogJob_ = Run(kCatalogLua);
    const ULONGLONG now = GetTickCount64();
    if (!invJob_.valid() && !action_.valid() && now - lastPoll_ >= 500) {
        lastPoll_ = now;
        invJob_ = Run(kInventoryLua);
    }
}

void ItemEditor::Act(std::string code, std::string message)
{
    message_ = std::move(message);
    action_ = Run(std::move(code));
}

void ItemEditor::Add(const std::string& id, int level)
{
    if (action_.valid())
        return;
    // 照游戏购买的做法：复制定义、设等级、刷新效果文字、插到末尾、分配面板键 'p' .. _itemPanelNextId
    std::string code =
        "if type(ui) ~= 'table' then return '游戏界面还没准备好' end\n"
        "local I = require('src.game.item')\n"
        "local d = I.getById(" + Quote(id) + ")\n"
        "if not d then return '没有这个物品 id' end\n"
        "local maxL = math.max(1, math.floor(tonumber(d.maxLevel) or 1))\n"
        "local L = math.max(1, math.min(maxL, " + std::to_string(level) + "))\n"
        "ui._itemPurchased = ui._itemPurchased or {}\n"
        "for _, o in ipairs(ui._itemPurchased) do\n"
        "  if tostring(o.id) == tostring(d.id) then\n"
        "    o.level = L\n"
        "    if type(o.updateEffectString) == 'function' then pcall(o.updateEffectString, o) end\n"
        "    return 'ok'\n"
        "  end\n"
        "end\n"
        "local o = {}\n"
        "for k, v in pairs(d) do o[k] = v end\n"
        "o.level = L\n"
        "if type(o.updateEffectString) == 'function' then pcall(o.updateEffectString, o) end\n"
        "table.insert(ui._itemPurchased, o)\n"
        "ui._itemPurchasedKeys = ui._itemPurchasedKeys or {}\n"
        "local n = tonumber(ui._itemPanelNextId) or (#ui._itemPurchased + 100)\n"
        "table.insert(ui._itemPurchasedKeys, 'p' .. tostring(n))\n"
        "ui._itemPanelNextId = n + 1\n"
        "return 'ok'\n";
    const Def* d = Find(id);
    const std::string name = d ? d->name : id;
    bool owned = false;
    for (const Owned& o : inventory_)
        owned |= o.id == id;
    Act(std::move(code), (owned ? "已把「" + name + "」改成 " : "已放进背包：「" + name + "」") +
                             (owned ? std::to_string(level) + " 级" : " " + std::to_string(level) + " 级"));
}

void ItemEditor::SetLevel(int index, const std::string& id, int level)
{
    if (action_.valid())
        return;
    std::string code = CheckSlot(index, id) +
        "local maxL = math.max(1, math.floor(tonumber(o.maxLevel) or 1))\n"
        "o.level = math.max(1, math.min(maxL, " + std::to_string(level) + "))\n"
        "if type(o.updateEffectString) == 'function' then pcall(o.updateEffectString, o) end\n"
        "return 'ok'\n";
    const Def* d = Find(id);
    Act(std::move(code), "已把「" + (d ? d->name : id) + "」改成 " + std::to_string(level) + " 级");
}

void ItemEditor::Remove(int index, const std::string& id)
{
    if (action_.valid())
        return;
    // 背包和面板键是两条并行的列表，一起删
    std::string code = CheckSlot(index, id) +
        "table.remove(list, " + std::to_string(index) + ")\n"
        "if type(ui._itemPurchasedKeys) == 'table' and ui._itemPurchasedKeys[" + std::to_string(index) + "] then\n"
        "  table.remove(ui._itemPurchasedKeys, " + std::to_string(index) + ")\n"
        "end\n"
        "return 'ok'\n";
    const Def* d = Find(id);
    Act(std::move(code), "已删除「" + (d ? d->name : id) + "」（不退水晶）");
}
