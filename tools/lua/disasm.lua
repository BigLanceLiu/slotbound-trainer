-- 最小的 LuaJIT 2.1 字节码反汇编（游戏去掉了 jit.bc，只剩 jit.util）
-- 用法：把 TARGET 换成取函数的表达式，slotbound_luacmd -f disasm.lua
local f = (...TARGET...)
local fu = require('jit.util')
local NAMES = "ISLT  ISGE  ISLE  ISGT  ISEQV ISNEV ISEQS ISNES ISEQN ISNEN ISEQP ISNEP ISTC  ISFC  IST   ISF   ISTYPEISNUM MOV   NOT   UNM   LEN   ADDVN SUBVN MULVN DIVVN MODVN ADDNV SUBNV MULNV DIVNV MODNV ADDVV SUBVV MULVV DIVVV MODVV POW   CAT   KSTR  KCDATAKSHORTKNUM  KPRI  KNIL  UGET  USETV USETS USETN USETP UCLO  FNEW  TNEW  TDUP  GGET  GSET  TGETV TGETS TGETB TGETR TSETV TSETS TSETB TSETM TSETR CALLM CALL  CALLMTCALLT ITERC ITERN VARG  ISNEXTRETM  RET   RET0  RET1  FORI  JFORI FORL  IFORL JFORL ITERL IITERLJITERLLOOP  ILOOP JLOOP JMP   FUNCF IFUNCFJFUNCFFUNCV IFUNCVJFUNCVFUNCC FUNCCW"
local function name(op) return (NAMES:sub(op * 6 + 1, op * 6 + 6):gsub(' ', '')) end
local function kstr(i) local c = fu.funck(f, -i - 1); return type(c) == 'string' and string.format('%q', c) or '<' .. type(c) .. '>' end
local out = {}
local info = fu.funcinfo(f)
for pc = 1, info.bytecodes - 1 do
  local ins = fu.funcbc(f, pc)
  if not ins then break end
  if ins < 0 then ins = ins + 4294967296 end -- funcbc 返回有符号 32 位
  local op = ins % 256
  local a = math.floor(ins / 256) % 256
  local c = math.floor(ins / 65536) % 256
  local b = math.floor(ins / 16777216)
  local d = math.floor(ins / 65536)
  local n = name(op)
  local s
  if n == 'TGETS' or n == 'TSETS' then s = string.format('%s %d %d %s', n, a, b, kstr(c))
  elseif n == 'GGET' or n == 'GSET' or n == 'KSTR' or n == 'ISEQS' or n == 'ISNES' then s = string.format('%s %d %s', n, a, kstr(d))
  elseif n == 'KNUM' or n == 'ISEQN' or n == 'ISNEN' then s = string.format('%s %d %s', n, a, tostring(fu.funck(f, d)))
  elseif n == 'KSHORT' then s = string.format('%s %d %d', n, a, d >= 32768 and d - 65536 or d)
  elseif n == 'UGET' then s = string.format('%s %d up%d(%s)', n, a, d, tostring(debug.getupvalue(f, d + 1)))
  elseif n:find('^JMP') or n:find('FOR') or n:find('LOOP') or n == 'ITERL' then s = string.format('%s %d => %d', n, a, pc + 1 + d - 32768)
  elseif n:find('VN$') or n:find('NV$') then s = string.format('%s %d %d %s', n, a, b, tostring(fu.funck(f, c)))
  elseif n:find('^IS') or n == 'MOV' or n == 'NOT' or n == 'UNM' or n == 'LEN' or n == 'KPRI' or n == 'KNIL' or n:find('^RET') or n == 'TNEW' or n == 'TDUP' or n == 'FNEW' or n == 'UCLO' then s = string.format('%s %d %d', n, a, d)
  else s = string.format('%s %d %d %d', n, a, b, c) end
  out[#out + 1] = string.format('%04d %s', pc, s)
end
return table.concat(out, '\n')
