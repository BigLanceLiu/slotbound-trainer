#include "luaclient.h"

#include <windows.h>

#include <cstdint>
#include <mutex>
#include <string>

#include "protocol.h"

namespace bridge {

namespace {

bool ReadAll(HANDLE h, void* buf, DWORD size)
{
    auto* p = static_cast<uint8_t*>(buf);
    while (size) {
        DWORD got = 0;
        if (!ReadFile(h, p, size, &got, nullptr) || !got)
            return false;
        p += got;
        size -= got;
    }
    return true;
}

} // namespace

bool Available()
{
    return WaitNamedPipeW(kPipeName, 0) || GetLastError() == ERROR_SEM_TIMEOUT;
}

Result Execute(const std::string& code, unsigned timeoutMs)
{
    // DLL 那边是单实例管道，一次只服务一个连接，回完话再重建。修改器里几个后台任务
    // （读目录、刷新单位、改词条）会同时发请求 → 本进程内先排队，连接时再对「忙 / 正在重建」重试。
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);

    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (;;) {
        pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
            break;
        const DWORD err = GetLastError();
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline)
            return { false, err == ERROR_FILE_NOT_FOUND ? "连不上游戏里的 slotbound_lua.dll（没注入？）"
                                                        : "管道一直忙，稍后再试" };
        if (err == ERROR_PIPE_BUSY)
            WaitNamedPipeW(kPipeName, static_cast<DWORD>(deadline - now));
        else if (err == ERROR_FILE_NOT_FOUND)
            Sleep(20); // 服务端刚断开、还没重建
        else
            return { false, "打开管道失败（错误 " + std::to_string(err) + "）" };
    }

    Result r;
    const uint32_t len = static_cast<uint32_t>(code.size());
    DWORD put = 0;
    uint32_t outLen = 0;
    if (!WriteFile(pipe, &len, 4, &put, nullptr) || !WriteFile(pipe, code.data(), len, &put, nullptr)) {
        r.text = "发送失败";
    } else if (!ReadAll(pipe, &outLen, 4) || outLen == 0 || outLen > kMaxMessage) {
        r.text = "没收到回复";
    } else {
        std::string reply(outLen, '\0');
        if (!ReadAll(pipe, reply.data(), outLen)) {
            r.text = "回复不完整";
        } else {
            r.ok = reply[0] == '0';
            r.text = reply.substr(1);
        }
    }
    CloseHandle(pipe);
    return r;
}

} // namespace bridge
