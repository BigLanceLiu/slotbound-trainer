#include "unitlevel.h"

#include <windows.h>

#include <cstdlib>

#include "luaops.h"

namespace {

using namespace luaops;

// 一行：sym 品级 星级 阵容序号 locked/last 吸收LV 最高星 门槛(逗号) 待选职业 待升星 职业名 生命 攻击 攻速 射程
constexpr const char* kUnitLua = R"lua(
if not u then return 'none' end
local FS = '\31'
local E = require('src.data.constants').evolution or {}
local th = E.rankThresholds and E.rankThresholds[u.grade] or {}
local maxR = E.maxRankByGrade and E.maxRankByGrade[u.grade] or 1
local ths = {}
for r = 2, maxR do ths[#ths+1] = tostring(th[r] or 0) end
local st = u.stats or {}
return table.concat({ tostring(u.sym), tostring(u.grade), tostring(u.rank or 1), tostring(u.index or 0), how,
  tostring(u.absorbLV or 1), tostring(maxR), table.concat(ths, ','), u.needsClassSelect and '1' or '0',
  tostring(u.pendingRank or 0), tostring(u.subclassName or ''),
  tostring(st.hp or 0), tostring(st.atk or 0), tostring(st.aspd or 0), tostring(st.distance or 0) }, FS)
)lua";

} // namespace

void UnitLevelEditor::Update(bool visible)
{
    if (Ready(unitJob_)) {
        const bridge::Result r = unitJob_.get();
        unit_ = {};
        if (r.ok && r.text != "none") {
            const auto f = Split(r.text, kFs);
            if (f.size() >= 15) {
                unit_.valid = true;
                unit_.sym = f[0];
                unit_.grade = f[1];
                unit_.rank = atoi(f[2].c_str());
                unit_.rosterIndex = atoi(f[3].c_str());
                unit_.locked = f[4] == "locked";
                unit_.absorbLv = atoi(f[5].c_str());
                unit_.maxRank = atoi(f[6].c_str());
                if (!f[7].empty())
                    for (const std::string& t : Split(f[7], ','))
                        unit_.thresholds.push_back(atoi(t.c_str()));
                unit_.needsClassSelect = f[8] == "1";
                unit_.pendingRank = atoi(f[9].c_str());
                unit_.subclass = f[10];
                unit_.hp = atof(f[11].c_str());
                unit_.atk = atof(f[12].c_str());
                unit_.aspd = atof(f[13].c_str());
                unit_.range = atof(f[14].c_str());
            }
            error_.clear();
        } else if (!r.ok) {
            error_ = r.text;
        }
    }
    if (Ready(action_)) {
        const std::string err = ActionError(action_.get());
        if (!err.empty())
            message_ = err;
        lastPoll_ = 0;
    }

    if (!visible)
        return;
    const ULONGLONG now = GetTickCount64();
    if (!unitJob_.valid() && !action_.valid() && now - lastPoll_ >= 500) {
        lastPoll_ = now;
        unitJob_ = Run(std::string(kResolveUnit) + kUnitLua);
    }
}

void UnitLevelEditor::Act(std::string code, std::string message)
{
    message_ = std::move(message);
    action_ = Run(std::move(code));
}

void UnitLevelEditor::SetLevel(int level)
{
    if (action_.valid() || !unit_.valid || level < 1)
        return;
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "local f = rawget(_G, '__slotSetAwakeningLevel')\n"
        "if type(f) ~= 'function' then return '游戏里找不到 __slotSetAwakeningLevel' end\n"
        "f(u, " + std::to_string(level) + ")\n"
        "return 'ok'\n";
    Act(std::move(code), "吸收 LV 已改成 " + std::to_string(level));
}

void UnitLevelEditor::RaiseRank(int rank)
{
    if (action_.valid() || !unit_.valid || rank <= unit_.rank)
        return;
    std::string code = CheckSameUnit(unit_.sym, unit_.rosterIndex) +
        "local f = rawget(_G, '__slotStoneRaiseRank')\n"
        "if type(f) ~= 'function' then return '游戏里找不到 __slotStoneRaiseRank' end\n"
        "f(u, " + std::to_string(rank) + ")\n"
        "return 'ok'\n";
    Act(std::move(code), "已申请升到 " + std::to_string(rank) + " 星：在游戏里选职业后生效");
}
