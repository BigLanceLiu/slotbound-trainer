#pragma once

#include <string>
#include <vector>

#include "lua_editor.h"

// 单位等级：星级（rank，游戏叫「转职」）和吸收 LV（absorbLV），以及基础属性、技能冷却 / 触发条件。
// 都调游戏自己的函数或改存档里的字段，属性由游戏重算。
//
//   __slotSetAwakeningLevel(u, lv)   设吸收 LV，并按「已算进属性的 LV(_absorbLvApplied) → 新 LV」补属性差；可升可降
//   __slotStoneRaiseRank(u, 目标星)  命运石升星：LV 不够先补到门槛，再走升星流程 → 游戏弹出转职选择，选完才真正升星；只能往上
//   constants.evolution.rankThresholds[品级][星] = 升到该星需要的 LV；maxRankByGrade[品级] = 最高星级（目前都是 3）
class UnitLevelEditor : public LuaEditor {
public:
    enum class SkillKind { None, Active, Passive };

    struct Param {
        std::string key;                 // charges / chance / internalCooldown
        double base = 0, current = 0;
    };
    struct Unit {
        bool valid = false;
        std::string sym, grade, subclass;
        int rank = 1, rosterIndex = 0, maxRank = 1, pendingRank = 0;
        int absorbLv = 1;
        bool locked = true;
        bool needsClassSelect = false;   // 升星后等玩家选职业
        std::vector<int> thresholds;     // [i] = 升到 i+2 星需要的 LV
        double hp = 0, atk = 0, aspd = 0, range = 0;
        SkillKind skill = SkillKind::None;
        std::string skillName;
        double skillBaseCd = 0;          // 技能定义里的冷却（秒）
        double skillCdMul = 1;           // 现在的冷却倍率（含修改器的自定义）
        double skillNaturalMul = 1;      // 游戏原倍率：22 号词条「技能加速」
        bool skillCdCustom = false;      // 单位上有修改器的 __sbSkillCdMul
        std::vector<Param> skillParams;  // 被动技能的触发条件

        bool HasActiveCd() const { return skill == SkillKind::Active && skillBaseCd > 0; }
        double SkillCd() const { return skillBaseCd * skillCdMul; }
        double NaturalCd() const { return skillBaseCd * skillNaturalMul; }
    };

    // 冷却最短 0.1 秒（再短就每帧放技能了）
    static constexpr double kMinCdSeconds = 0.1;

    void SetLevel(int level);
    void RaiseRank(int rank);

    // 改基础属性 unit.stats[key]（hp / atk / aspd / distance）：每场开战时游戏按它生成战场单位，存档里也是它 → 一直有效。
    // 正在战斗中的话，战场单位按同样的比例一起改（maxhp / dmg / atkPeriod / distance），当场生效，词条等加成照样乘在上面。
    void SetStat(const char* key, double value, const char* label);

    // 主动技能冷却设成 seconds 秒（最短 kMinCdSeconds；负数 = 恢复原版）。
    // 22 号词条「技能加速」的数值被游戏规整到固定档位，没法任意定 → 改成在单位上存倍率 __sbSkillCdMul，
    // 由修改器包住 __slotGetSkillCooldownMul 乘上去（luaops::kCdHookLua）。被动技能不走这个倍率。
    void SetSkillCooldown(double seconds);

    // 被动技能触发条件：改单位的覆盖表 __sbSkillRt[key]（reset = 去掉，恢复原版），由 awakening.runtime 包装生效
    void SetSkillParam(const std::string& key, double value, bool reset, const std::string& label);

    const Unit& Selected() const { return unit_; }

private:
    std::string PollCode() const override;
    void OnPoll(const bridge::Result& r) override;

    Unit unit_;
};
