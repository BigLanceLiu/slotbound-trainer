#pragma once

#include <string>

// 修改器 exe 里内嵌了 slotbound_lua.dll（构建时 cmake/embed.cmake 转成数组），发出去只要一个 exe。
// 注入前释放到 %LOCALAPPDATA%\slotbound-trainer\slotbound_lua_<内容哈希>.dll：
//   同一版本的文件已经在了就直接用（游戏可能还加载着它，不能覆盖）；文件名带哈希，新版本换新文件。
namespace bridge {

// 成功返回 DLL 的完整路径；失败返回空串，error 里是原因
std::wstring ExtractEmbeddedDll(std::string& error);

} // namespace bridge
