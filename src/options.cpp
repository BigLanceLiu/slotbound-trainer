#include "options.h"

#include <windows.h>

#include <cstdlib>

#include "luaops.h"

namespace {

using namespace luaops;

// 词条目录：id  名字  稀有度  来源  效果  已解锁  备注  档位(value:chance|...)
constexpr const char* kCatalogLua = R"lua(
local RO = require('src.data.randomoption')
local Lang = require('src.data.language')
local loc = Lang[rawget(_G, 'currentGameLang') or 'zh'] or Lang.zh or {}
local FS = '\31'
local out = {}
for _, d in ipairs(RO.catalog) do
  local id = tostring(d.id)
  local notes = {}
  local src = ({ Spawn = '出生时', Promotion = '晋升时', Both = '出生 / 晋升' })[tostring(d.source)]
  if src then notes[#notes+1] = src end
  if d.requiresBasicAttack then notes[#notes+1] = '需要普通攻击' end
  if d.requiresRangedBasicAttack then notes[#notes+1] = '需要远程普攻' end
  if d.requiresActiveSkill then notes[#notes+1] = '需要主动技能' end
  if d.requiresAttackSkill then notes[#notes+1] = '需要攻击技能' end
  if d.requiresBountySource then notes[#notes+1] = '需要能拿赏金' end
  if d.requiresNudge then notes[#notes+1] = '需要本局开微调' end
  if d.requiresDeploymentRoom then notes[#notes+1] = '需要空余单位名额' end
  if d.incompatibleWith then notes[#notes+1] = '与 ' .. tostring(d.incompatibleWith) .. ' 互斥' end
  if type(d.permittedGrades) == 'table' then
    local g = {}
    for k, v in pairs(d.permittedGrades) do g[#g+1] = type(k) == 'number' and tostring(v) or tostring(k) end
    table.sort(g)
    local zh = { normal = '普通', advanced = '高级', rare = '稀有', special = '特殊', legendary = '传说', prism = '棱彩' }
    for i, x in ipairs(g) do g[i] = zh[x] or x end
    notes[#notes+1] = '仅限 ' .. table.concat(g, '/') .. ' 品级单位'
  end
  local vars = {}
  for _, v in ipairs(d.variants or {}) do
    if v.value ~= nil then vars[#vars+1] = tostring(v.value) .. ':' .. tostring(v.chance or 0) end
  end
  -- 要传定义表：传字符串 id 时游戏函数查不到 initiallyUnlocked，永远返回 true
  local ok, unlocked = pcall(RO.isUnlocked, d)
  out[#out+1] = table.concat({ id,
    loc['random_option.name.' .. id] or tostring(d.name),
    tostring(d.rarity), tostring(d.source),
    loc['random_option.effect.' .. id] or tostring(d.effect),
    (ok and unlocked == false) and '0' or '1',
    table.concat(notes, '，'), table.concat(vars, '|') }, FS)
end
return table.concat(out, '\n')
)lua";

// 选中单位：第一行 sym  品级  等级  阵容序号  locked/last
//   S 行：槽位  id  档位值
//   E 行：词条 id  不适用原因（空 = 适用）  这个单位已有几个
// 适用条件照抄游戏抽词条（RO.rollForSpawn / 晋升时）用的检查：品级、普攻、远程普攻、主动技能、攻击技能、赏金、微调、互斥、解锁
constexpr const char* kUnitLua = R"lua(
if not u then return 'none' end
local FS = '\31'
local Skill = require('src.data.skill')
local function try(f, ...) if type(f) ~= 'function' then return nil end local ok, r = pcall(f, ...) if ok then return r end return nil end
local out = { table.concat({ tostring(u.sym), tostring(u.grade), tostring(u.rank or 0), tostring(u.index or 0), how }, FS) }
local have = {}
for i, o in ipairs(u.randomOptions or {}) do
  out[#out+1] = table.concat({ 'S', tostring(i), tostring(o.id), o.variant and tostring(o.variant.value) or '' }, FS)
  have[tostring(o.id)] = (have[tostring(o.id)] or 0) + 1
end
local caps = {
  basic = try(RO.canBasicAttack, u), ranged = try(RO.canRangedBasicAttack, u), bounty = try(RO.canEarnBounty, u),
  active = try(rawget(_G, '__slotChoiceHasActiveSkill'), u), attack = try(Skill.hasAttackSkillForUnit, u),
  nudge = try(RO.isNudgeEnabledForRun),
}
for _, d in ipairs(RO.catalog) do
  local id = tostring(d.id)
  local why = {}
  if try(RO.isUnlocked, d) == false then why[#why+1] = '升级树里还没解锁' end
  if type(d.permittedGrades) == 'table' then
    local ok = false
    for k, v in pairs(d.permittedGrades) do if (type(k) == 'number' and v or k) == u.grade then ok = true end end
    if not ok then why[#why+1] = '单位品级不符' end
  end
  if d.requiresBasicAttack and caps.basic == false then why[#why+1] = '该兵种没有普通攻击' end
  if d.requiresRangedBasicAttack and caps.ranged == false then why[#why+1] = '该兵种不是远程普攻' end
  if d.requiresActiveSkill and caps.active == false then why[#why+1] = '该兵种没有主动技能' end
  if d.requiresAttackSkill and caps.attack == false then why[#why+1] = '该兵种没有攻击技能' end
  if d.requiresBountySource and caps.bounty == false then why[#why+1] = '该兵种拿不到赏金' end
  if d.requiresNudge and caps.nudge == false then why[#why+1] = '本局没开微调' end
  if d.incompatibleWith and have[tostring(d.incompatibleWith)] then why[#why+1] = '与已有词条 ' .. tostring(d.incompatibleWith) .. ' 互斥' end
  out[#out+1] = table.concat({ 'E', id, table.concat(why, '，'), tostring(have[id] or 0) }, FS)
end
return table.concat(out, '\n')
)lua";

} // namespace

const OptionEditor::Def* OptionEditor::Find(const std::string& id) const
{
    for (const Def& d : catalog_)
        if (d.id == id)
            return &d;
    return nullptr;
}

void OptionEditor::Update(bool visible)
{
    if (Ready(catalogJob_)) {
        const bridge::Result r = catalogJob_.get();
        if (!r.ok) {
            error_ = "读词条目录失败：" + r.text;
        } else {
            catalog_.clear();
            for (const std::string& line : Split(r.text, '\n')) {
                const auto f = Split(line, kFs);
                if (f.size() < 8)
                    continue;
                Def d{ f[0], f[1], f[2], f[3], f[4], f[6], f[5] != "0", {} };
                if (!f[7].empty())
                    for (const std::string& v : Split(f[7], '|')) {
                        const size_t colon = v.rfind(':');
                        d.variants.push_back({ v.substr(0, colon), colon == std::string::npos ? 0 : atof(v.c_str() + colon + 1) });
                    }
                catalog_.push_back(std::move(d));
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
            if (head.size() >= 4) {
                unit_.valid = true;
                unit_.sym = head[0];
                unit_.grade = head[1];
                unit_.rank = atoi(head[2].c_str());
                unit_.rosterIndex = atoi(head[3].c_str());
                unit_.locked = head.size() < 5 || head[4] == "locked";
                for (size_t i = 1; i < lines.size(); ++i) {
                    const auto f = Split(lines[i], kFs);
                    if (f.size() >= 4 && f[0] == "S")
                        unit_.slots.push_back({ atoi(f[1].c_str()), f[2], f[3] });
                    else if (f.size() >= 4 && f[0] == "E")
                        unit_.fit[f[1]] = { f[2], atoi(f[3].c_str()) };
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

void OptionEditor::SetOption(int slot, const std::string& id, const std::string& variant)
{
    if (action_.valid() || !unit_.valid)
        return;
    // 先核对选中的还是界面上显示的那个单位，再生成词条实例放进槽位
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "local o = RO.rollForId(" + Quote(id) + ")\n" +
        // rollForId 对升级树里没解锁的词条返回 nil → 照它的格式用目录定义自己拼一个实例
        "if not o then\n"
        "  local d\n"
        "  for _, x in ipairs(RO.catalog) do if tostring(x.id) == " + Quote(id) + " then d = x end end\n"
        "  if not d then return '目录里没有这个词条 id' end\n"
        "  o = { id = d.id, name = d.name, effect = d.effect, icon = d.icon, rarity = d.rarity, source = d.source }\n"
        "  local vs, total = d.variants or {}, 0\n"
        "  for _, v in ipairs(vs) do total = total + (tonumber(v.chance or v.weight) or 0) end\n"
        "  local r, pick = math.random() * total, vs[#vs]\n"
        "  for _, v in ipairs(vs) do\n"
        "    r = r - (tonumber(v.chance or v.weight) or 0)\n"
        "    if r <= 0 then pick = v break end\n"
        "  end\n"
        // 和游戏一致：有档位表就一定有 variant；档位没有值（24/25/32 这类）时是空表 {}
        "  if pick then o.variant = { value = pick.value } end\n"
        "end\n";
    if (!variant.empty())
        code += "o.variant = { value = " + Quote(variant) + " }\n";
    code += "u.randomOptions = u.randomOptions or {}\n"
            "local slot = math.min(" + std::to_string(slot) + ", #u.randomOptions + 1)\n"
            "u.randomOptions[slot] = o\n"
            "u.randomOption = u.randomOptions[1]\n"
            "return 'ok'\n";
    const Def* d = Find(id);
    const std::string what = "「" + (d ? d->name : id) + (variant.empty() ? "" : " " + variant) + "」";
    message_ = slot > static_cast<int>(unit_.slots.size()) ? "已新增词条" + what
                                                           : "已把第 " + std::to_string(slot) + " 个词条换成" + what;
    action_ = Run(std::move(code));
}

void OptionEditor::RemoveOption(int slot)
{
    if (action_.valid() || !unit_.valid)
        return;
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "if u.randomOptions and u.randomOptions[" + std::to_string(slot) + "] then table.remove(u.randomOptions, " +
        std::to_string(slot) + ") end\n" +
        "u.randomOption = u.randomOptions and u.randomOptions[1] or nil\n"
        "return 'ok'\n";
    message_ = "已删除第 " + std::to_string(slot) + " 个词条";
    action_ = Run(std::move(code));
}
