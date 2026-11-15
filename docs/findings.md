# 逆向记录

## 引擎

- LÖVE，`lua51.dll` 是 **LuaJIT 2.1，GC64 模式**（64 位指针，TValue 8 字节）。

## LuaJIT GC64 内存布局（修改器用到的部分）

### TValue（8 字节）

- 数字：直接是 IEEE double。判断：`(uint32)((int64)bits >> 47) <= 0xFFFEFFFF`
- 其他：`(tag << 47) | 指针`
  - 字符串：`0xFFFD800000000000 | GCstr*`
  - 表：`0xFFFA000000000000 | GCtab*`

### GCstr（字符串对象）

| 偏移 | 内容 |
| --- | --- |
| +0x09 | gct = 4（字符串） |
| +0x14 | 长度 u32 |
| +0x18 | 字符数据，结尾 `\0` |

字符串是驻留的，同一个名字只有一个对象。**找引用时要用对象地址（数据地址 − 0x18）**，用数据地址会一个都找不到。

### 哈希节点 Node（0x18 字节）

| 偏移 | 内容 |
| --- | --- |
| +0x00 | value（TValue） |
| +0x08 | key（TValue） |
| +0x10 | next |

表对象 GCtab：+0x09 gct = 0xB4，+0x18 是节点数组指针。

## 金币

数据流：

```
coins（Lua 全局，真正的来源）
  → 动画表.target
  → 每帧插值
  → displayCoins（全局）
  → ui.goldBalance
```

`coins` 下游的都是每帧重算的显示值，改了会被立刻覆盖。只改 `coins`。

### 定位方法

1. 在可读写的私有内存里找 `"coins\0"`，校验 +9 = 4、+0x14 = 5，得到字符串对象 S
2. 扫 8 字节对齐的 `0xFFFD800000000000 | S`，命中处减 8 就是节点
3. 只留 value 是数字的节点。同名字段很多（实测 2000~2800 个，别的表、旧快照，值都一样），
   用邻居区分：`_G` 里的那个节点 ±512 个节点内有键 `displayCoins` 的节点，实测唯一
4. 每帧读之前核对节点 +8 的键还是 `"coins"`，不是就重新定位（`_G` 扩容 rehash 会搬节点）

为什么不找基址：Lua 的根（`lua_State` / `global_State`）在堆上、C 栈上，没有稳定的静态指针链；
按名字找每次 0.5 秒，比维护指针链省事得多，游戏更新也不怕。

## 旋转次数

- 全局 `spinsLeft`（double），节点就在 `_G` 里，离 `coins` 约 340 个节点。
- 同名字段共 3 个：`_G` 的那个、另一个表里的一份（那个表里也有 `coins`，像状态快照；改 `_G` 后它不跟着变）、一个旧的。
  所以邻居要用 `displayCoins`（只有 `_G` 有），用 `coins` 当邻居会挑出两个。
- 验证：改 5 → 6，界面显示 6，转一次变 5。

## 水晶

- 全局 `jungsu`（韩语 정수 = 精华，游戏里英文叫 Essence），在 `_G` 里紧挨着 `coins`。改成 99 后用户花到 82，游戏照常扣，确认是来源。
- 显示链：`jungsu` → 动画表 `jungsuBalanceTarget` → `jungsuBalance`（插值）→ 全局 `displayJungsu`。
  **和金币不同：动画目标不是每帧从 `jungsu` 取，只在游戏自己加减水晶时更新**，所以只改 `jungsu` 界面不动；
  直接写 `displayJungsu` 会被下一帧覆盖。修改器改 `jungsu` 时同时写 `jungsuBalanceTarget`（`LuaVar` 的 mirror），界面就会滚过去。
  `essence` 字段（6 个）是从 `jungsu` 同步出去的副本。
- 同名字段 8 个左右，`displayCoins` 邻居挑出唯一的 `_G` 那个。
- 找法：不知道名字时，直接把 `_G` 整张表的「字符串键 → 数字」列出来（scratchpad `lua_gtab.py <pid> <coins节点>`），
  GC64 GCtab 的 node 指针在 +0x28、hmask 在 +0x34；`_G` 的 hmask 是 1023，数字全局约 80 个，按游戏里的数一对就出来。

## 词条（random option）

- 模块 `src.data.randomoption`（`require` 得到）：`catalog`（32 条定义）、`rollForId(id)` 生成实例、`isUnlocked(id)`、`rollForSpawn` 等。
  定义：`id`、`name`（韩文）、`effect`、`icon`、`rarity`（common/rare/legend）、`source`（Spawn/Promotion/Both）、
  `variants = { {value='+10%', chance=0.05}, ... }`，部分有 `requiresBasicAttack`、`incompatibleWith`、`permittedGrades`。
- 实例：`{ id, name, effect, icon, rarity, source, variant = { value = '+10%' } }`。
- 单位：`battlefield.rosterUnits[i]`；词条列表 `unit.randomOptions`，`unit.randomOption` 是第 1 个。
- 选中：全局 `tooltip`（信息面板）。`tooltip.locked` 时 `lockedTarget = { kind='roster', ref=单位 }`；面板关了只剩 `tooltip.lastSelectedRef`。
- 生效：`battlefield.beginBattle` 按 `randomOptions` 调 `computeRandomOptionAtkMul/HpMul/AspdMul/...` 算倍率缓存到单位
  （`_randomOptionAtkMul` 等），`endBattle` 清掉。战斗外改，下一场生效；赏金、狩猎本能等是战斗中实时查 `randomOptions`。
  验证：把 D 的「孤独 +40%」换成「战斗本能 +10%」，`computeRandomOptionAtkMul(D) = 1.1`，用户确认界面显示。
- 文本：`require('src.data.language')[currentGameLang]['random_option.name.21']` = '孤独'，`random_option.effect.NN` 是效果。
  （`language.getText` 返回韩文，不用它。）

### 词条规则（2026-11-14 用游戏函数实测）

- **重复**：游戏抽词条时排除已有 id（`rollForSpawnExcluding`），正常不会重复；但 `computeRandomOption*Mul` 逐个相乘，
  手动塞重复会叠加：战斗本能 +10% ×1 / ×2 / ×3 = 1.1 / 1.21 / 1.331。
- **兵种限制**（抄自 `rollForSpawn` 的常量）：`requiresBasicAttack`（`RO.canBasicAttack`）、`requiresRangedBasicAttack`
  （`RO.canRangedBasicAttack`）、`requiresActiveSkill`（全局 `__slotChoiceHasActiveSkill(u)`）、`requiresAttackSkill`
  （`require('src.data.skill').hasAttackSkillForUnit(u)`）、`requiresBountySource`（`RO.canEarnBounty`）、`requiresNudge`
  （`RO.isNudgeEnabledForRun()`）、`permittedGrades`（单位品级）、`incompatibleWith`（14 ↔ 32）、`source`（Spawn 出生 / Promotion 晋升 / Both）。
  `RO.isContextEligible` 对什么都返回 true，不能用来判断。
- **数量上限**：数据上没有。`randomOptions` 是普通列表，结算、各词条效果（`*.has`、`computeRandomOption*Mul`）都 `ipairs` 遍历全部。
  游戏自己给词条的地方：出生 `rollForSpawn` / `RO.rollList(n)`（n 被截到 5，实测 6~12 都返回 5 个）、3 星转职选择（`table.insert`）、石头、佣兵合同。
  修改器不再卡 3 个；超过 5 个时提示面板可能显示不全（未实测）。
- **解锁**：11/14/23/25/26/28/29/32 这 8 个定义里 `initiallyUnlocked = false`，要在升级树（`src.ui.upgrade.isUnlockNodeOwned`）里解锁。
  没解锁时 `RO.rollForId` 直接返回 nil。`RO.isUnlocked` 必须传**定义表**：传字符串 id 它读不到 `initiallyUnlocked`，永远返回 true（我一开始就是这么被骗的）。
  修改器对没解锁的词条按目录定义自己拼实例（字段和 `rollForId` 的一样；档位按 chance 加权抽，档位没值时 `variant = {}`，和游戏一致）。
- **两套「品级」**：词条稀有度 common/rare/legend（普通/稀有/传说）；单位品级 normal/advanced/rare/special/legendary/prism
  （`language.zh['grade.label.*']`：普通/高级/稀有/特殊/传说/棱彩）。`permittedGrades` 用的是单位品级。

## 单位等级（星级 / 吸收 LV）

- 两种等级：`unit.rank` 星级（游戏叫「转职」，2 星得技能、3 星选职业分支）；`unit.absorbLV` 吸收 LV（吞单位涨，成就到 100）。
- **星级跟着 LV 走**：`constants.evolution.rankThresholds[品级][星]` 是升星需要的 LV（normal 5/12、advanced 6/14、rare/special 8/18、
  legendary 10/20、prism 16/28）；`maxRankByGrade` 目前都是 3。`rankMultipliers` 是星级属性倍率。
- `__slotSetAwakeningLevel(u, lv)`（反汇编）：`_absorbLvApplied` 没有就用当前 LV，设 `absorbLV = lv`，再调上值函数按
  「`_absorbLvApplied` → `absorbLV`」补属性差。可升可降、可逆：假单位 LV1→4 生命 100→105，改回 1 精确还原。
- `__slotStoneRaiseRank(u, 目标星)`（命运石升星，反汇编）：目标 ≤ 当前星或超过最高星就不做；LV 不到门槛先 `__slotSetAwakeningLevel` 补到门槛；
  再调上值 2（升星流程）→ 设 `needsClassSelect`、`pendingRank`、`classSelectTargetRank`，**rank 当场不变**，游戏弹转职选择，选完才升星。
  没有降星流程。
- 修改器「单位」页（`src/unitlevel.cpp`）直接调这两个函数。

## 单位基础属性

- 阵容单位 `u.stats = { hp, atk, aspd, distance, speed }`（另有顶层 `u.distance`），存档保存。词条的生命/攻击/攻速/射程/移速倍率
  **已经乘进 `u.stats`**：词条变化时游戏按「新倍率 ÷ 旧倍率（`_randomOptionAtkMul` 等缓存）」调整 stats。
- 开战 `beginBattle`（反汇编 pc 658–725）按 stats 生成战场单位 `u.allyRef`：`maxhp = max(1, floor(hp + 0.5))`、`hp = maxhp`、
  `dmg = max(0, atk)`、`distance = distance`、`atkPeriod = 1 / max(0.05, aspd)`、`atkTicks` 由 atkPeriod 换算，打上 `_statsFromRoster`。
- 修改器改 `u.stats[key]` → 一直有效；战斗中（`battlefield.mode == 'battle'` 且 allyRef 在 `battlefield.allies` 里）按同比例改战场单位。
  移速不做（用户只要一直有效的四项）。升星时是否保留改过的值未验证。

## 技能冷却

- 技能定义 `require('src.data.skill').definitions[id]`（125 个）：`cooldown` 秒、`activationType`（active / passive）、`runtime.cooldown`（优先）、
  `runtime.minRank`。按职业归属，`getSkillsForUnit(u)` 取单位的技能。中文名 `language.zh['skill.name.<id>']`。
- 战场单位 `a.skill.cd` = 冷却，`a.skill.cdT` = 剩余冷却；`a.skill._randomOptionBaseCooldown` = 基础冷却。
- `__slotApplySkillCooldownOption(a)`（开战、词条结算时调）：`a.skill.cd = 基础 × __slotGetSkillCooldownMul(a)`。
  面板显示的 `awakening.liveCooldown(u, def)` 也乘它，再四舍五入到 0.1。两处都是每次现查 `_G`。
- `__slotGetSkillCooldownMul(u)` 只看 22 号词条「技能加速」：每条 × `(1 - min(95, |值|)/100)`，最低 0.05。
  **但 `RO.normalizeOption` 会把 22 号的值规整到 8/10/12/14/16% 几档**（10/15/16/17/19 号也会被规整），所以不能靠自定义词条数值改冷却。
- 修改器做法（`luaops::kCdHookLua`）：包住全局 `__slotGetSkillCooldownMul`，在原倍率上再乘阵容单位的 `__sbSkillCdMul`；
  传进来的是战场单位时，用阵容单位的 `allyRef` 反查（`beginBattle` 先设 allyRef（pc 630），后算 skill（pc 1102），顺序没问题）。
  `RS.copySerializable` 不过滤字段 → `__sbSkillCdMul` 跟存档保存；修改器没开时没人包，等于原版冷却。
  老虎机每秒的同步顺带检查包装还在不在。验证：假战场单位要 4 / 1 / 0.1 秒 → `skill.cd` 4 / 1 / 0.1，面板 4 / 1 / 0.1；不设 → 8。

## 觉醒（awakening）

- 面板上单位头像下的 I / II / III 图标（如「终结 III」）是觉醒，不是词条。`A = require('src.game.awakening')`。
- 单位 `u.awakening = { version = 10, unitId = 'W09', selected = { 'W12', 'W28', 'W05' }, levels, stage, seen, pending }`。
  职业身份 `A.identity(u)`（看 subclassKey / subclassName / sym）；`A.data.units[身份].candidates` = 这个职业的 12 个候选（`A.pool(u)`）；
  总目录 `A.catalog(u).candidates`（177 个），定义有 `stages[1..5].text`、`values[1..5]`（如终结 W12：20/25/30/35/40）。
  中文：`language.zh['awakening.10.<id>.name']`、`['awakening.10.<id>.<级>.text']`。
- 节点 `A.data.milestones`：吸收 LV 30 / 60 / 100（3 星）、160（4 星）、250（5 星）；最高 3 星 → 容量 3。
- **第 10 版等级规则**（反汇编 `A.level`）：id 在 selected 里就 = `min(5, #selected)`，`levels` 表不起作用 → 选了几个，每个就是几级。
  `A.choose` 选一个后把所有已选的 levels 都设成 stage。`A.has` 只看 selected + 总目录，不看是不是本职业的。
- `A.normalize(u)`：去重、只留总目录里有的、最多 5 个，重设 stage / levels；不看容量。效果（has / value）都是战斗中实时查，
  选的时候不往 stats 里写 → 直接改 selected 安全、立即生效，存档保存。
- 验证（合成的幻影单位）：W12/W28/W05 都是 III 级（终结 30%）→ 加 W01、W02 → 全部 V 级（终结 40%）；加第 6 个被截掉；删 1 个 → 全部 IV 级。
- 修改器「单位 → 觉醒」子页（`src/awakening.cpp`）：只列本职业候选，新增 / 替换 / 删除，改完调 `A.normalize`。

## 物品（商店）

- 定义 `require('src.game.item')`：`getAll()` 63 件（战斗 55、训练手册 8），`getById(id)`；字段 id、price、rarity C/U/R/L、
  maxLevel（C 3 / U 2 / R、L 1）、typeKo（전투 / 훈련）、initiallyUnlocked、stage（all 或第几关）、各级数值表（如 `damageReduction = {0.1, 0.15, 0.2}`）、
  `updateEffectString`。中文 `language.zh['item.name.<id>']`、`['item.effect.<id>.<级>']`。
- 背包 `ui._itemPurchased[i]`（定义的浅拷贝 + level），面板键 `ui._itemPurchasedKeys[i] = 'p' .. ui._itemPanelNextId`（两条并行列表）。
  商店 `ui._itemOffers[1..3]`、本轮已买 `ui._itemShopPurchased[格]`、刷新价 `ui._itemRerollCost`、轮次 `ui._itemOfferCycleId`。
- 购买（反汇编 `love.mousepressed` pc 5251–5719）：本轮买过就不行 → 找已有同 id → 已满级拒绝 → 新物品且 `#背包 >= getItemInventoryCapacity()` 拒绝 →
  价格（上值函数，含折扣 `__slotShopDiscountFor`）> `jungsu` 拒绝 → 扣水晶 → 已有：`level = min(满级, level + 1)`；新：复制、level 1、
  `updateEffectString`、插入、分配面板键。**没有 apply 钩子**，效果在战斗里实时查背包（如 `__slotGetSturdyFistBasicAttackDamageMul`）。
- `getItemInventoryCapacity() = min(8, 6 + 加成)`，界面最多 8 格。读档 `restorePurchasedItems` 按 id + level 用 `getById` 重建。
- 修改器「物品」页（`src/items.cpp`）：背包改等级 / 删除（不退水晶），全部物品直接放进背包（已有就改等级），等级压到满级以内。
  验证（假背包）：加拳套 → 改 3 级（效果文字 12%）、团结项链要 5 级被压成 1、没解锁的满溢圣杯照样能加，面板键 p11/p12/p13 连续。

## 被动技能触发条件

- 被动技能只有 10 个：闪光斩击 / 闪光乱舞（`FlashSlash`，`runtime.charges = 7` 次普攻）、梦魇连击 / 灾厄连击（`NightmareFlurry`，
  普攻时 `chance = 0.2`、`internalCooldown = 2`）、暗影收割（击杀，`internalCooldown = 3`）、毁灭震荡（击杀，`cooldown = 5`）、
  胜利信号 / 胜利之翼（击杀或打首领充能 4 格）、救赎之箭 / 天界归还（倒下的友军复活）。
- 参数都从 `awakening.runtime(a)` 取：`a.primarySkillDef.runtime`（同技能单位**共用这张表**），有觉醒时合并成副本缓存在 `a._awakeningRuntime`。
  `unit_abilities.<技能>.runtime` 每次都 `require('src.game.awakening').runtime(a)` 现取，所以包 `A.runtime` 就能覆盖。
  例：`flash_slash.charges(a) = max(1, floor(runtime.charges 或 7))`；`onBasic` 每次普攻 `skill.flashCharge = min(charges, +1)`。
- 修改器：单位上存 `__sbSkillRt = { charges=, chance=, internalCooldown= }`，包装返回「原参数 + 覆盖」的副本，按 (单位, 原表, 覆盖表) 缓存，
  改设置整张换表。验证（假战场单位）：闪光乱舞 charges 7 → 1 / 3，barFill 2/3 = 0.667；梦魇连击 chance 0.2 → 1、internalCooldown 2 → 0.5；
  共用定义仍是 7。毁灭震荡的冷却、胜利信号的充能不走 runtime 的这几个参数，没做。

## 老虎机

- 状态在全局 `slot`（`src.game.slot.new` 造的对象，`package.loaded['src.game.slot']` 是类）。
  `slot.grid[行][列]`：当前 3×3；`slot.reelStrips[1..3]`：三条 64 格转轮带；`slot.reelStripPos`；`slot.result`（winningLines、won、_jackpot…）。
- 符号：W 战士、A 弓手、S 枪兵、M 法师、H 骑兵、D 决斗者（王冠图标）；`J` 王牌（Joker）、`Blank` 空。
  `buildResultGrid` 上值 4 = `{W,A,S,M,H,D}`，上值 5 按 `symbolWeights` 加权抽。
- **小丑 `J`**：万能符号（`joker_symbol_tooltip` = 视为所有单位符号），转轮带上没有，靠核心「小丑种子」（每格 1.5%）、
  `_coreSpinMods.jokerChance` / `forceJokerCells` 放进盘面。计奖（`slot.evaluateWin` 的上值 1，参数 grid → 中奖线、符号、symbolsByLine）实测：
  `J A A` 这条线算 A；整条 `J J J` 的线符号是 `Joker` → `battlefield.spawn` 出「小丑」单位（광대 / Jester）；
  9 格全 J = 8 条 Joker 线，`isJackpotGrid` 也是 true；全 A 中间一个 J 仍是 A 的大奖。
- 中奖线 `slot:getWinningLinesDefinition()`：3 横 + 3 竖 + 2 斜，共 8 条；9 格全同 = 大奖（`isJackpotGrid`）。
- **结果在拉杆时就定了**：`startSpin` 先调 `slot:buildResultGrid()` 得到整盘 3×3（`pendingGrid`），再把各转轮的
  `targetFinalSymbols` 设成它，转轮动画只是滚到这个结果。所以包一层 `buildResultGrid` 改返回值，画面和结算都一致。
  游戏自己的「核心」道具也是从这里影响：`slot._coreSpinMods` 的 `forceCells`、`forceJokerCells`、`forceWinProb`、`jokerChance`。
- 修改器的大奖模式（`src/slotctl.cpp`）：包装读 `_G.__sbSlotCfg`，关着时原样透传；`__sbSlotWrapped`（弱键表）标记已包过的函数，
  每秒检查一次，游戏换了 `slot` 对象就重新包。随机兵种用 `math.random`，不碰 `love.math.random`（游戏自己的随机序列）。
  验证：直接调 `slot:buildResultGrid()`，开 H → `HHH/HHH/HHH`，`isJackpotGrid = true`；关 → 正常随机。

## 刻印（imprint）

和词条是两套东西。中文界面叫「刻印」，单位面板上那个「扩散」就是刻印 `transfer`。

- 定义：`require('src.data.constants').imprint.definitions[id]`，33 个，每个 5 级：`name`、`tag`（buff/spawn/body/debuff）、
  `condition`、`icon`、`levels[L] = { desc, shortDesc, stats }`。中文：`language.zh['imprint.name.<id>']`、`['imprint.desc.<id>_<L>']`。
- 实例：`unit.imprints[i]` = 定义字段摊平 + `level`、`maxLevel`、`rarity`、`stats`（**就是 `levels[level].stats` 那张表**）、`desc`、`shortDesc`。
  游戏的生成函数是 `require('src.game.battlefield').signatureImprint` 的上值 3：`mk(定义, 稀有度)`，只生成 1 级；
  改等级 = 换 `level` + 换成那一级的 `stats/desc/shortDesc`。`signatureImprint(unit)` 返回兵种 + 品级天生的刻印。
- 槽数：`unit.maxImprintSlots`，按 `constants.imprint.slots`（normal 1 / advanced 2 / rare 3 / special 3 / legendary 4 / prism 5）。
  反汇编 `beginBattle` 的上值 13：只在 `_gradeStartApplied ~= grade`（品级变了）时重设槽数，**不会删多出来的刻印**。
- 数量没有上限：效果代码 `pairs(u.imprints)` 按 id 匹配、按 `level` 去定义里取数（例：`combat.execute.params`、`combat.lifesteal.rate`）。
  验证：假单位上放吸血 2 级 → `lifesteal.rate = 0.06`；处决 3 级 → 阈值 0.14。
- 反汇编工具：游戏去掉了 `jit.bc`，`tools/lua/disasm.lua` 用 `jit.util.funcbc` 自己解码。

## Lua 桥（注入）

- 管道是单实例：同时来多个连接会 `ERROR_PIPE_BUSY`，服务端回完话重建的间隙是 `ERROR_FILE_NOT_FOUND`。
  客户端（`luaclient.cpp`）进程内加锁排队，连接时对这两种错误重试到超时。
- `love.dll` 从 `lua51.dll` 导入 `lua_pushnumber`，游戏每帧经它调用（`love.timer.step` 等）→ IAT 挂钩，在主线程（游戏窗口线程）执行代码。
- 字节码去了调试信息（`debug.getinfo` 看不到参数名、行号 0），但 `jit.util.funck` 能读函数常量：
  `tools/lua/find_const.lua` 按字符串常量找函数（找到了 `beginBattle` 用 `_randomOptionAtkMul`）。

## 怎么找新变量名

`Slotbound.dat`（134 MB）头是 `SBDAT008`，自定义加密包，读不到 Lua 源码。
但游戏跑起来后变量名都是驻留字符串（GCstr），在堆里按正则扫 +9 = 4、+0x14 = 长度、结尾 `\0` 的对象就能全列出来：
搜 `spin|roll` 得到 539 个，其中 `spinsLeft`、界面文字 "Spins Left" 一对就对上了。
再对候选名字扫一遍节点、按当前游戏里的数值过滤，通常只剩一两个。

## 用字节码确认是谁写的（以后找新变量用）

在可疑地址下写断点，命中时 LuaJIT 解释器的寄存器：

| 寄存器 | 含义 |
| --- | --- |
| RSI | PC（下一条字节码） |
| RDI | KBASE（常量表） |
| RDX | BASE（栈帧） |
| RBX | DISPATCH |

- 字节码 4 字节：`op, A, C, B`（或 `op, A, D`）
- 字符串常量 idx 在 `KBASE − 8*(idx+1)`，数字常量在 `KBASE + 8*idx`
- 常用操作码：GGET 0x36、GSET 0x37、TGETV 0x38、TGETS 0x39、TSETV 0x3C、TSETS 0x3D、KSTR 0x27、KSHORT 0x29、KNUM 0x2A、ADDVV 0x20、SUBVV 0x21、CALL 0x42
- 往回解码 PC 前几条，看到 `GSET A, "coins"` 就知道写的是哪个全局

## 走过的弯路

- 一次改 4 个地址，用户同时花了钱，误判成「都是镜像」。**一次只改一个地址。**
- 验证水晶时按扫描时的 4 写了 5，但中间游戏里已经涨到 7，反而扣了 2（已改回 7）。**验证写入用相对改动（读当前值 +1），不写死数。**
- 找字符串引用时用了数据地址（差 0x18），结果 0 引用。
- 追静态基址：Lua 堆上的数据没有静态基址，方向错了。
