#pragma once

#include <windows.h>

#include <string>

// 把 slotbound_lua.dll 注入游戏 / 通知它卸载。命令行注入器和修改器共用。
namespace bridge {

// DLL 已经在游戏里（卸载事件存在）
bool Injected();

// LoadLibraryW 远程线程注入。dllPath 为空时用本 exe 同目录的 slotbound_lua.dll
bool Inject(DWORD pid, std::wstring dllPath, std::string& error);

// 通知 DLL 恢复导入表并自己卸载
bool Unload();

} // namespace bridge
