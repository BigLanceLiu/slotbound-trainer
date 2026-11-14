#include "imprints.h"

#include <windows.h>

#include <cstdlib>

#include "luaops.h"

namespace {

using namespace luaops;

// 刻印目录：id  名字  tag  各级效果（| 分隔）
constexpr const char* kCatalogLua = R"lua(
local C = require('src.data.constants')
local Lang = require('src.data.language')
local loc = Lang[rawget(_G, 'currentGameLang') or 'zh'] or Lang.zh or {}
local FS = '\31'
local ids = {}
for id in pairs(C.imprint.definitions) do ids[#ids+1] = id end
table.sort(ids, function(a, b) return (loc['imprint.name.' .. a] or a) < (loc['imprint.name.' .. b] or b) end)
local out = {}
for _, id in ipairs(ids) do
  local d = C.imprint.definitions[id]
  local lv = {}
  for L = 1, #(d.levels or {}) do
    local s = loc['imprint.desc.' .. id .. '_' .. L] or (d.levels[L] and d.levels[L].desc) or ''
    lv[#lv+1] = (tostring(s):gsub('[\n|\31]', ' '))
  end
  out[#out+1] = table.concat({ id, loc['imprint.name.' .. id] or tostring(d.name), tostring(d.tag), table.concat(lv, '|') }, FS)
end
return table.concat(out, '\n')
)lua";

// 选中单位：第一行 sym  品级  等级  阵容序号  locked/last  槽数  天生刻印 id
//   S 行：槽位  id  等级
constexpr const char* kUnitLua = R"lua(
if not u then return 'none' end
local FS = '\31'
local sig = ''
local B = package.loaded['src.game.battlefield']
if B and B.signatureImprint then
  local ok, r = pcall(B.signatureImprint, u)
  if ok and r then sig = type(r) == 'table' and tostring(r.id or '') or tostring(r) end
end
local out = { table.concat({ tostring(u.sym), tostring(u.grade), tostring(u.rank or 0), tostring(u.index or 0), how,
  tostring(u.maxImprintSlots or 1), sig }, FS) }
for i, im in ipairs(u.imprints or {}) do
  out[#out+1] = table.concat({ 'S', tostring(i), tostring(im.id), tostring(im.level or 1) }, FS)
end
return table.concat(out, '\n')
)lua";

} // namespace

const ImprintEditor::Def* ImprintEditor::Find(const std::string& id) const
{
    for (const Def& d : catalog_)
        if (d.id == id)
            return &d;
    return nullptr;
}

void ImprintEditor::Update(bool visible)
{
    if (Ready(catalogJob_)) {
        const bridge::Result r = catalogJob_.get();
        if (!r.ok) {
            error_ = "读刻印目录失败：" + r.text;
        } else {
            catalog_.clear();
            for (const std::string& line : Split(r.text, '\n')) {
                const auto f = Split(line, kFs);
                if (f.size() < 4)
                    continue;
                catalog_.push_back({ f[0], f[1], f[2], Split(f[3], '|') });
            }
            error_.clear();
        }
    }
    if (Ready(unitJob_)) {
        const bridge::Result r = unitJob_.get();
        unit_ = {};
        if (r.ok && r.text != "none") {
            const auto lines = Split(r.text, '\n');
            const auto head = Split(lines[0], kFs);
            if (head.size() >= 7) {
                unit_.valid = true;
                unit_.sym = head[0];
                unit_.grade = head[1];
                unit_.rank = atoi(head[2].c_str());
                unit_.rosterIndex = atoi(head[3].c_str());
                unit_.locked = head[4] == "locked";
                unit_.maxSlots = atoi(head[5].c_str());
                unit_.signature = head[6];
                for (size_t i = 1; i < lines.size(); ++i) {
                    const auto f = Split(lines[i], kFs);
                    if (f.size() >= 4 && f[0] == "S")
                        unit_.slots.push_back({ atoi(f[1].c_str()), f[2], atoi(f[3].c_str()) });
                }
            }
        } else if (!r.ok) {
            error_ = r.text;
        }
    }
    if (Ready(action_)) {
        const std::string err = ActionError(action_.get());
        if (!err.empty())
            message_ = err;
        lastPoll_ = 0; // 马上刷新显示
    }

    if (!visible)
        return;
    if (catalog_.empty() && !catalogJob_.valid())
        catalogJob_ = Run(kCatalogLua);
    const ULONGLONG now = GetTickCount64();
    if (!unitJob_.valid() && !action_.valid() && now - lastPoll_ >= 500) {
        lastPoll_ = now;
        unitJob_ = Run(std::string(kResolveUnit) + kUnitLua);
    }
}

void ImprintEditor::Act(std::string code, std::string message)
{
    message_ = std::move(message);
    action_ = Run(std::move(code));
}

void ImprintEditor::Set(int slot, const std::string& id, int level)
{
    if (action_.valid() || !unit_.valid)
        return;
    // 实例照游戏的做法生成：signatureImprint 的上值里有个 mk(定义, 稀有度)（只会生成 1 级），
    // 再把 level / stats / desc 换成目标等级的；找不到 mk 就按同样的字段自己拼
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "local C = require('src.data.constants')\n"
        "local d = C.imprint.definitions[" + Quote(id) + "]\n"
        "if not d then return '没有这个刻印 id' end\n"
        "local L = math.max(1, math.min(" + std::to_string(level) + ", #(d.levels or {})))\n"
        "local im = u.imprints or {}\n"
        "u.imprints = im\n"
        "local slot = math.min(" + std::to_string(slot) + ", #im + 1)\n"
        "local old = im[slot]\n"
        "local rarity = (old and old.rarity) or u.grade or 'normal'\n"
        "local mk\n"
        "local B = package.loaded['src.game.battlefield']\n"
        "if B and B.signatureImprint then\n"
        "  local fu = require('jit.util')\n"
        "  for n = 1, 16 do\n"
        "    local nm, f = debug.getupvalue(B.signatureImprint, n)\n"
        "    if not nm then break end\n"
        "    if type(f) == 'function' and fu.funcinfo(f).params == 2 then mk = f end\n"
        "  end\n"
        "end\n"
        "local o\n"
        "if mk then local ok, r = pcall(mk, d, rarity); if ok and type(r) == 'table' and r.id == d.id then o = r end end\n"
        "if not o then\n"
        "  o = {}\n"
        "  for k, v in pairs(d) do if k ~= 'levels' then o[k] = v end end\n"
        "  o.rarity = rarity\n"
        "  o.maxLevel = #(d.levels or {})\n"
        "end\n"
        "local lv = d.levels and d.levels[L]\n"
        "o.level = L\n"
        "if lv then o.stats = lv.stats; o.desc = lv.desc or o.desc; o.shortDesc = lv.shortDesc or o.shortDesc end\n"
        "im[slot] = o\n"
        // 游戏面板按 maxImprintSlots 画格子；不够就加，免得多出来的看不见
        "if (u.maxImprintSlots or 1) < #im then u.maxImprintSlots = #im end\n"
        "return 'ok'\n";
    const Def* d = Find(id);
    const std::string what = "「" + (d ? d->name : id) + " " + std::to_string(level) + " 级」";
    Act(std::move(code), slot > static_cast<int>(unit_.slots.size()) ? "已新增刻印" + what
                                                                     : "已把第 " + std::to_string(slot) + " 个刻印设为" + what);
}

void ImprintEditor::Remove(int slot)
{
    if (action_.valid() || !unit_.valid)
        return;
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "if u.imprints and u.imprints[" + std::to_string(slot) + "] then table.remove(u.imprints, " +
        std::to_string(slot) + ") end\n"
        "return 'ok'\n";
    Act(std::move(code), "已删除第 " + std::to_string(slot) + " 个刻印");
}

void ImprintEditor::SetMaxSlots(int slots)
{
    if (action_.valid() || !unit_.valid || slots < 1)
        return;
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "u.maxImprintSlots = " + std::to_string(slots) + "\n"
        "u.imprints = u.imprints or {}\n"
        "return 'ok'\n";
    Act(std::move(code), "刻印槽数已改成 " + std::to_string(slots));
}
