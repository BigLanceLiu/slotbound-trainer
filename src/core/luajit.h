#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "process.h"

// 游戏用的 lua51.dll 是 LuaJIT 2.1（GC64 模式）。这里只放外部读写 Lua 数据要用到的那点布局，
// 每一条都在游戏里实测核对过，过程见 docs/findings.md。
namespace lj {

// TValue 是 8 字节：数字就是原样的 double；其他类型 = (类型标签 << 47) | 对象地址
constexpr uint64_t kTagStr = 0xFFFD800000000000ull; // 字符串
constexpr uint64_t kTagTab = 0xFFFA000000000000ull; // 表
constexpr uint64_t kPtrMask = 0x00007FFFFFFFFFFFull;

// 字符串对象（GCstr）：+9 类型字节（4 = 字符串），+0x14 长度，+0x18 起是字符，末尾带 '\0'
constexpr uintptr_t kStrHeader = 0x18;
constexpr uint8_t kStrGct = 4;

// 表的哈希节点：+0 值，+8 key，+0x10 next（同一个桶链上的下一个节点）
constexpr uintptr_t kNodeSize = 0x18;

inline bool IsNumber(uint64_t bits)
{
    // 带标签的值右移 47 位（算术移位）后 >= 0xFFFF0000；double（含负数）都小于它
    return static_cast<uint32_t>(static_cast<int64_t>(bits) >> 47) <= 0xFFFEFFFFu;
}

inline double AsDouble(uint64_t bits)
{
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

inline uint64_t FromDouble(double d)
{
    uint64_t bits;
    std::memcpy(&bits, &d, sizeof(bits));
    return bits;
}

// 一个表字段（哈希节点）。表扩容会把节点搬走，所以每次用之前先 StillValid 一下
struct Field {
    uintptr_t node = 0; // 值就在 node 处
    uint64_t key = 0;   // 带标签的 key = kTagStr | 字段名字符串对象的地址

    bool Valid() const { return node != 0; }
};

// 节点还在原地、值还是数字
bool StillValid(const GameProcess& game, const Field& field);
bool ReadNumber(const GameProcess& game, const Field& field, double& out);
bool WriteNumber(const GameProcess& game, const Field& field, double value);

// 定位全局变量 name（值是数字）：扫堆找它的字符串对象 → 找引用它的 key 槽 → 取值是数字的节点。
// 别的表里也可能有同名字段，neighbor 填另一个肯定在 _G 里的全局变量名，用来挑出 _G 里那一个
// （_G 的节点在同一个数组里，彼此相距不远）。扫一遍堆大约一秒，别在 UI 线程里调。
bool LocateGlobalNumber(const GameProcess& game, const std::string& name, const std::string& neighbor, Field& out,
    std::string& log);

} // namespace lj
