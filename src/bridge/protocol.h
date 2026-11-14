#pragma once

// 修改器 / 命令行 ↔ 游戏内 slotbound_lua.dll 的通信约定。
// 字节流命名管道，一问一答：
//   请求：u32 长度 + UTF-8 Lua 代码（一个 chunk，可以 return 多个值）
//   回复：u32 长度 + 1 字节状态（'0' 成功 / '1' 出错）+ 文本
//         成功时文本是各返回值用 \t 连起来（字符串、数字、布尔、nil 原样；其他类型写 "table: 0x..."）
namespace bridge {

inline constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\slotbound_lua";
inline constexpr wchar_t kUnloadEvent[] = L"Local\\slotbound_lua_unload";
inline constexpr unsigned kMaxMessage = 16u << 20;

} // namespace bridge
