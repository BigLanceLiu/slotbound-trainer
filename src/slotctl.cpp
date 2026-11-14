#include "slotctl.h"

#include <windows.h>

#include <cstdlib>

#include "luaops.h"

namespace {

using namespace luaops;

// 装包装（已装就跳过）+ 写设置；回：hooked/noslot  已改次数  当前 grid 9 个字母
// 用 rawget/rawset 碰 _G：游戏到处用 rawget(_G, ...)，怕 _G 上有严格模式元表
constexpr const char* kSyncLua = R"lua(
local cfg = rawget(_G, '__sbSlotCfg')
if type(cfg) ~= 'table' then cfg = {}; rawset(_G, '__sbSlotCfg', cfg) end
cfg.jackpot = JACKPOT
cfg.symbol = SYMBOL
local s = rawget(_G, 'slot')
if type(s) ~= 'table' or type(s.buildResultGrid) ~= 'function' then return 'noslot\t0\t' end
local wrapped = rawget(_G, '__sbSlotWrapped')
if type(wrapped) ~= 'table' then wrapped = setmetatable({}, { __mode = 'k' }); rawset(_G, '__sbSlotWrapped', wrapped) end
if not wrapped[s.buildResultGrid] then
  local orig = s.buildResultGrid
  local function w(self, ...)
    local g = orig(self, ...)
    local c = rawget(_G, '__sbSlotCfg')
    if c and c.jackpot and type(g) == 'table' then
      local pick = c.symbol
      if pick == 'random' then
        local list = { 'W', 'A', 'S', 'M', 'H', 'D' }
        pick = list[math.random(#list)] -- 不用 love.math.random，免得动游戏自己的随机序列
      end
      for r = 1, 3 do
        g[r] = g[r] or {}
        for col = 1, 3 do g[r][col] = pick end
      end
      c.hits = (c.hits or 0) + 1
    end
    return g
  end
  wrapped[w] = true
  s.buildResultGrid = w
end
local cells = {}
for r = 1, 3 do for col = 1, 3 do
  local v = s.grid and s.grid[r] and s.grid[r][col]
  cells[#cells+1] = type(v) == 'string' and v:sub(1, 1) or '?'
end end
return 'hooked\t' .. tostring(cfg.hits or 0) .. '\t' .. table.concat(cells)
)lua";

std::string Replace(std::string s, const std::string& from, const std::string& to)
{
    const size_t pos = s.find(from);
    if (pos != std::string::npos)
        s.replace(pos, from.size(), to);
    return s;
}

} // namespace

void SlotControl::Update(bool injected, bool jackpot, const std::string& symbol)
{
    if (Ready(job_)) {
        const bridge::Result r = job_.get();
        if (!r.ok) {
            error_ = r.text;
            hooked_ = false;
        } else {
            error_.clear();
            const auto f = Split(r.text, '\t');
            hooked_ = f[0] == "hooked";
            hits_ = f.size() > 1 ? atoi(f[1].c_str()) : 0;
            grid_ = f.size() > 2 ? f[2] : std::string();
        }
    }
    if (!injected) {
        hooked_ = false;
        return;
    }
    // 设置变了马上同步；否则每秒一次（检查包装还在不在、读当前结果）
    const ULONGLONG now = GetTickCount64();
    const bool changed = jackpot != sentJackpot_ || symbol != sentSymbol_;
    if (job_.valid() || (!changed && now - lastSync_ < 1000))
        return;
    lastSync_ = now;
    sentJackpot_ = jackpot;
    sentSymbol_ = symbol;
    std::string code = Replace(kSyncLua, "JACKPOT", jackpot ? "true" : "false");
    code = Replace(code, "SYMBOL", Quote(symbol));
    job_ = Run(std::move(code));
}
