#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iterator>

namespace {

// GetPrivateProfileString 不会去掉行尾的 "; 注释"，这里自己处理
std::wstring ReadStr(const std::wstring& path, const wchar_t* section, const wchar_t* key, const wchar_t* def)
{
    wchar_t buf[256];
    GetPrivateProfileStringW(section, key, def, buf, static_cast<DWORD>(std::size(buf)), path.c_str());

    std::wstring s = buf;
    if (auto semi = s.find(L';'); semi != std::wstring::npos)
        s.resize(semi);
    const auto first = s.find_first_not_of(L" \t");
    const auto last = s.find_last_not_of(L" \t");
    return first == std::wstring::npos ? std::wstring{} : s.substr(first, last - first + 1);
}

int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int def)
{
    std::wstring s = ReadStr(path, section, key, L"");
    return s.empty() ? def : static_cast<int>(wcstol(s.c_str(), nullptr, 0)); // base 0：认 0x 前缀
}

bool ReadBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool def)
{
    std::wstring s = ReadStr(path, section, key, L"");
    if (s.empty())
        return def;
    return _wcsicmp(s.c_str(), L"true") == 0 || s == L"1";
}

void WriteStr(const std::wstring& path, const wchar_t* section, const wchar_t* key, const std::wstring& value)
{
    WritePrivateProfileStringW(section, key, value.c_str(), path.c_str());
}

void WriteKey(const std::wstring& path, const wchar_t* section, const wchar_t* key, int vk)
{
    wchar_t buf[16];
    swprintf_s(buf, L"0x%02X", vk);
    WriteStr(path, section, key, buf);
}

void WriteInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int value)
{
    WriteStr(path, section, key, std::to_wstring(value));
}

void WriteBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool value)
{
    WriteStr(path, section, key, value ? L"true" : L"false");
}

void LoadVar(const std::wstring& path, const wchar_t* section, VarConfig& v)
{
    v.lock = ReadBool(path, section, L"lock", v.lock);
    v.lockValue = ReadInt(path, section, L"lock_value", v.lockValue);
    v.addAmount = ReadInt(path, section, L"add_amount", v.addAmount);
    v.lockKey = ReadInt(path, section, L"lock_key", v.lockKey);
    v.addKey = ReadInt(path, section, L"add_key", v.addKey);
}

void SaveVar(const std::wstring& path, const wchar_t* section, const VarConfig& v)
{
    WriteBool(path, section, L"lock", v.lock);
    WriteInt(path, section, L"lock_value", v.lockValue);
    WriteInt(path, section, L"add_amount", v.addAmount);
    WriteKey(path, section, L"lock_key", v.lockKey);
    WriteKey(path, section, L"add_key", v.addKey);
}

} // namespace

bool LoadConfig(const std::wstring& path, Config& cfg)
{
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;

    cfg.enabled = ReadBool(path, L"general", L"enabled", cfg.enabled);
    cfg.panicKey = ReadInt(path, L"general", L"panic_key", cfg.panicKey);
    cfg.menuKey = ReadInt(path, L"general", L"menu_key", cfg.menuKey);
    cfg.hideKey = ReadInt(path, L"general", L"hide_key", cfg.hideKey);
    cfg.exitKey = ReadInt(path, L"general", L"exit_key", cfg.exitKey);
    cfg.processName = ReadStr(path, L"general", L"process_name", cfg.processName.c_str());
    if (cfg.processName.empty())
        cfg.processName = L"Slotbound.exe";

    LoadVar(path, L"coins", cfg.coins);
    LoadVar(path, L"spins", cfg.spins);
    LoadVar(path, L"jungsu", cfg.jungsu);

    cfg.slotJackpot = ReadBool(path, L"slot", L"jackpot", cfg.slotJackpot);
    const std::wstring sym = ReadStr(path, L"slot", L"symbol", L"A");
    cfg.slotSymbol.clear();
    for (wchar_t c : sym) // 只会是 ASCII 字母
        cfg.slotSymbol += static_cast<char>(c);
    if (cfg.slotSymbol.empty())
        cfg.slotSymbol = "A";

    cfg.overlay = ReadBool(path, L"debug", L"overlay", cfg.overlay);
    return true;
}

bool SaveConfig(const std::wstring& path, const Config& cfg)
{
    WriteBool(path, L"general", L"enabled", cfg.enabled);
    WriteKey(path, L"general", L"panic_key", cfg.panicKey);
    WriteKey(path, L"general", L"menu_key", cfg.menuKey);
    WriteKey(path, L"general", L"hide_key", cfg.hideKey);
    WriteKey(path, L"general", L"exit_key", cfg.exitKey);
    WriteStr(path, L"general", L"process_name", cfg.processName);

    SaveVar(path, L"coins", cfg.coins);
    SaveVar(path, L"spins", cfg.spins);
    SaveVar(path, L"jungsu", cfg.jungsu);

    WriteBool(path, L"slot", L"jackpot", cfg.slotJackpot);
    WriteStr(path, L"slot", L"symbol", std::wstring(cfg.slotSymbol.begin(), cfg.slotSymbol.end()));

    WriteBool(path, L"debug", L"overlay", cfg.overlay);

    // WritePrivateProfileString 有缓存，传全空参数强制落盘
    return WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str()) != FALSE;
}
