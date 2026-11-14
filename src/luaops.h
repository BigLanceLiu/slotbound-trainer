#pragma once

// 经 Lua 桥在游戏里执行代码的公共小工具（词条页、刻印页共用）

#include <chrono>
#include <future>
#include <string>
#include <vector>

#include "bridge/luaclient.h"

namespace luaops {

// 回传文本：字段分隔 \x1f，记录分隔 \n
inline constexpr char kFs = '\x1f';

inline std::vector<std::string> Split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t pos = s.find(sep, start);
        out.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }
    return out;
}

// Lua 字符串字面量
inline std::string Quote(const std::string& s)
{
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\' || c == '"')
            out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

inline bool Ready(std::future<bridge::Result>& f)
{
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

inline std::future<bridge::Result> Run(std::string code)
{
    return std::async(std::launch::async, [code = std::move(code)] { return bridge::Execute(code, 3000); });
}

// 公共前缀：取当前点选的己方单位 → 局部变量 u（没有时 nil），how = 'locked' / 'last'
inline constexpr const char* kResolveUnit = R"lua(
local RO = require('src.data.randomoption')
local t = rawget(_G, 'tooltip')
local tg = t and t.locked and (t.lockedTarget or t.target) or nil
local u = (tg and tg.kind == 'roster') and tg.ref or nil
local how = 'locked'
-- 面板关掉后 lockedTarget 就清了，退回到「最近选中」的单位（还在阵容里才算）
if not u and t and t.lastSelectedRef and rawget(_G, 'battlefield') then
  for _, r in ipairs(battlefield.rosterUnits or {}) do
    if r == t.lastSelectedRef then u = r; how = 'last'; break end
  end
end
)lua";

// 核对选中的还是界面上显示的那个单位（防止点按钮前游戏里换了选中）
inline std::string CheckSameUnit(const std::string& sym, int rosterIndex)
{
    return std::string(kResolveUnit) + "if not u then return 'none' end\n" +
        "if tostring(u.sym) ~= " + Quote(sym) + " or (u.index or 0) ~= " + std::to_string(rosterIndex) +
        " then return 'changed' end\n";
}

// 改动类请求的回复 → 界面提示；成功返回空串
inline std::string ActionError(const bridge::Result& r)
{
    if (!r.ok)
        return "失败：" + r.text;
    if (r.text == "none")
        return "失败：没有选中单位";
    if (r.text == "changed")
        return "失败：选中的单位变了，请看一眼再点";
    if (r.text != "ok")
        return "失败：" + r.text;
    return {};
}

} // namespace luaops
