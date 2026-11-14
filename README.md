# 转运军团（Slotbound）修改器

外部程序：读写游戏进程内存，透明覆盖层画菜单（定制主题的 Dear ImGui + D3D11）。
**不注入 DLL、不改游戏代码、不改游戏文件**，只改一个 Lua 全局变量。

## 功能

| 功能 | 说明 |
| --- | --- |
| 显示金币 | 左上角状态栏实时显示 |
| 改金币 | 菜单「资源」页：输入数值「改成这个数」，或 +100 / +1000 / +10000 |
| 锁定金币 | 花掉的金币马上补回锁定值 |
| 快捷键加金币 | 每按一次加「每次加」设的数，默认不绑键 |
| 旋转次数 | 和金币一样：显示 / 改成 / +1 +5 +10 / 锁定 / 快捷键 |
| 水晶 | 同上（Lua 全局 `jungsu`） |
| 老虎机 | 「老虎机」页：大奖模式开关，打开后每次拉杆都是 9 格同一兵种（可选兵种或每次随机），显示当前结果。**需要注入** |
| 词条 | 「词条」页：看选中单位的词条，替换 / 删除 / 新增（不限个数），32 个词条全列出，可选数值档位。**需要注入**，见下 |
| 刻印 | 「刻印」页：看选中单位的刻印，替换 / 删除 / 新增、直接改等级（1～5），改刻印槽数，33 个刻印每级效果全列出。**需要注入** |

### 注入（「老虎机」「词条」「刻印」页用）

改老虎机结果、词条、刻印要在游戏里执行 Lua，外部读写内存做不到，所以这几页需要先点「注入」：把 `slotbound_lua.dll` 加载进游戏，
它把 `love.dll` 导入表里 `lua51.dll!lua_pushnumber` 的指针换成自己的函数，在游戏主线程上执行修改器发来的 Lua 代码
（命名管道 `\\.\pipe\slotbound_lua`），执行完把 Lua 栈恢复原样。只改这一个指针，不改代码字节、不改游戏文件；
点「卸载注入」或关游戏就恢复。金币 / 旋转 / 水晶不需要注入。

命令行工具（开发用）：`slotbound_inject.exe load|unload`、`slotbound_luacmd.exe "return coins"` / `-f 脚本.lua`。
注入期间 `build\Release\slotbound_lua.dll` 被游戏占用，改了 DLL 源码要先卸载再编译。

还没做：波次、基地血量、棱彩概率，见 [docs/plan.md](docs/plan.md)。

## 用法

1. 游戏开「窗口」或「无边框窗口」模式（独占全屏会盖住覆盖层）。
2. 运行 `slotbound_trainer.exe`，先开游戏还是先开修改器都行，它每秒找一次 `Slotbound.exe`。
3. 进到一局里，状态栏出现「金币 N」就是找到了（约 0.5 秒）。主菜单里没有金币变量，会每 5 秒重找一次。

| 默认按键 | 作用 |
| --- | --- |
| Insert | 打开/关闭菜单 |
| F12 | 总开关（急停：停止锁定，改金币按钮变灰） |
| F10 | 隐藏/显示覆盖层 |
| End | 退出修改器 |

按键在菜单「设置」页改，改动 0.5 秒后自动存进 `trainer.ini`。

**退出修改器时不会把金币改回去**——改出来的金币本来就是想留着的。

## 编译

```powershell
.\build.ps1          # 开发版，读 config\trainer.ini
.\build.ps1 -Dist    # 便携版，读 exe 旁边的 trainer.ini，复制到 dist\
```

需要 VS 2022（带 C++ 桌面开发）。产物在 `build\Release\`：

- `slotbound_trainer.exe`：修改器本体
- `slotbound_probe.exe`：命令行定位工具，排查用

```powershell
.\build\Release\slotbound_probe.exe coins --neighbor displayCoins            # 只读：找到并打印金币
.\build\Release\slotbound_probe.exe coins --neighbor displayCoins --set 5000 # 写入并读回
```

## 原理（一句话）

游戏是 LÖVE 引擎（LuaJIT 2.1），金币是 Lua 全局变量 `coins`。修改器在游戏堆里找到 `"coins"` 这个字符串对象，
再找键指向它的哈希节点，用旁边的全局变量 `displayCoins` 认出哪个是 `_G` 里的那一个，然后直接读写节点里的 double。
地址每次开游戏都变、`_G` 扩容时还会搬家，所以不存地址，每帧核对一次键还对不对，不对就重新找。
细节见 [docs/findings.md](docs/findings.md)。

## 为什么在这款游戏上做是 OK 的

- **纯单机**：没有联机对战、排行榜对比、交易市场，改自己的金币不影响任何其他玩家。
- **没有反作弊**：进程里没有反作弊模块，修改器只用 `ReadProcessMemory` / `WriteProcessMemory`，不绕过任何保护。
- **不碰正版验证**：不改 exe、不改游戏文件、不动 Steam 相关的任何东西；游戏照常要正版才能玩。
- **只改数据**：金币 / 旋转 / 水晶只写数值；词条 / 刻印功能要注入一个 DLL（用户 2026-11-14 同意），只在游戏里执行 Lua 改单位数据，不改游戏代码和文件。
- **自己的电脑、自己的存档**：建议改之前备份存档目录 `%APPDATA%\slotbound_full\`（`save_fallback.dat` 等）。
  这个目录开了 Steam 云同步（有 `steam_autocloud.vdf`），改过的存档会同步到云上，想留一份干净的就先备份。
  锁定金币可能让一局失去挑战性，想正常玩时关掉锁定即可。

## 目录

```
src/
  main.cpp      主循环、菜单、状态栏
  luavar.*      一个 Lua 全局数值（金币、旋转次数）：后台定位、每帧核对、改/锁
  luajit.*      LuaJIT 2.1 GC64 内存布局 + 按名字定位全局变量
  process.*     进程连接、读写（写权限按需申请）
  overlay.*     透明覆盖层（D3D11 + ImGui，定制主题）
  config.*      trainer.ini 读写
  probe.cpp     命令行定位工具
config/trainer.example.ini
docs/plan.md      分阶段计划
docs/findings.md  逆向记录
```
