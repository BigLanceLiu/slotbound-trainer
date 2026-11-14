-- 找出引用了某些字符串常量的 Lua 函数（字节码去掉了调试信息，但常量表还在）
-- 用法：把 NEEDLES 改成要找的常量，slotbound_luacmd -f find_const.lua
local NEEDLES = { ...NEEDLES... }
local fu = require('jit.util')
local seen, out, queue = {}, {}, {}
local function push(v, path, depth)
  if (type(v) == 'table' or type(v) == 'function') and not seen[v] then
    seen[v] = true
    queue[#queue + 1] = { v, path, depth }
  end
end
push(_G, '_G', 0)
push(package.loaded, 'loaded', 0)
push(debug.getregistry(), 'registry', 0)
local i = 1
while i <= #queue do
  local v, path, depth = queue[i][1], queue[i][2], queue[i][3]
  i = i + 1
  if type(v) == 'table' then
    if depth < 6 then
      for k, x in pairs(v) do push(x, path .. '.' .. tostring(k), depth + 1) end
    end
  else
    local info = fu.funcinfo(v)
    if info and info.gcconsts then
      local hits = {}
      for n = 1, info.gcconsts do
        local c = fu.funck(v, -n)
        if type(c) == 'string' then
          for _, needle in ipairs(NEEDLES) do
            if c == needle then hits[#hits + 1] = c end
          end
        elseif type(c) == 'proto' then
          -- 子函数原型：还没实例化成闭包的拿不到，跳过
        end
      end
      if #hits > 0 then out[#out + 1] = path .. '  [' .. table.concat(hits, ',') .. ']' end
    end
    -- 顺着上值找到更多函数 / 表（模块的 local 函数都挂在上值里）
    for n = 1, 255 do
      local name, up = debug.getupvalue(v, n)
      if not name then break end
      if depth < 8 then push(up, path .. '^' .. n, depth + 1) end
    end
  end
end
return table.concat(out, '\n')
