/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.

DeltaTimeTrace: hook Time.get_deltaTime, 记录每个调用点的 RVA + 特征码 + 调用次数,
写入文件 DeltaTimeTrace.log。用于定位"整数毫秒累加计时器"的调用链。
*/
#include "DeltaTimeTrace.h"
#include "../Config/Config.h"
#include <Windows.h>
#include "../MinHook/MinHook.h"
#include <intrin.h>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <string>
#include <iostream>

// 7.1 OS: Time.get_deltaTime static → RVA 0x19BE86F0
// 7.0 OS: 0x18F8A5E0
static constexpr uintptr_t kDeltaTimeRVA = 0x19BE86F0;

typedef float (__fastcall* tTimeGetDeltaTime)(void* methodInfo);
static tTimeGetDeltaTime o_TimeGetDeltaTime = nullptr;

// 调用点记录
struct CallSite
{
    uintptr_t rva = 0;
    uint64_t   count = 0;
    uint8_t    sig[32] = { 0 };
    bool       sigCaptured = false;
};

static std::mutex s_mtx;
static std::unordered_map<uintptr_t, CallSite> s_sites;
static uintptr_t s_gameBase = 0;
static bool s_enabled = false;

// 心跳节流
static uint64_t s_frameCounter = 0;
static constexpr uint64_t kHeartbeatInterval = 300; // 每 300 次调用 dump 一次

static std::string FormatHex(const uint8_t* data, int len)
{
    std::string out;
    out.reserve(len * 3);
    char buf[4];
    for (int i = 0; i < len; ++i)
    {
        snprintf(buf, sizeof(buf), "%02X ", data[i]);
        out += buf;
    }
    return out;
}

static void WriteLog(const std::string& line)
{
    std::ofstream f("DeltaTimeTrace.log", std::ios::app);
    if (f.is_open())
    {
        f << line << '\n';
        f.flush();
    }
}

static float __fastcall hk_TimeGetDeltaTime(void* methodInfo)
{
    float result = o_TimeGetDeltaTime ? o_TimeGetDeltaTime(methodInfo) : 0.0f;

    if (!s_enabled || s_gameBase == 0)
        return result;

    // _ReturnAddress() = 调用者的返回地址（call 指令后的下一条）
    void* retAddr = _ReturnAddress();
    uintptr_t callerRVA = (uintptr_t)retAddr - s_gameBase;

    bool isNew = false;
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        auto& site = s_sites[callerRVA];
        site.rva = callerRVA;
        site.count++;

        // 首次发现此调用点 → 读 32 字节特征码
        if (!site.sigCaptured)
        {
            // 从调用点前 8 字节开始读，覆盖 call 指令本身和上下文
            uintptr_t readStart = (uintptr_t)retAddr - 8;
            memcpy(site.sig, (const void*)readStart, 32);
            site.sigCaptured = true;
            isNew = true;
        }
    }

    if (isNew)
    {
        char buf[512];
        auto& site = s_sites[callerRVA];
        snprintf(buf, sizeof(buf),
            "[DT] NEW caller RVA=0x%llX count=%llu sig=%s",
            (unsigned long long)callerRVA,
            (unsigned long long)site.count,
            FormatHex(site.sig, 32).c_str());
        std::cout << buf << std::endl;
        WriteLog(buf);
    }

    // 心跳：定期全量 dump
    if (++s_frameCounter % kHeartbeatInterval == 0)
    {
        std::lock_guard<std::mutex> lk(s_mtx);
        WriteLog("--- heartbeat: " + std::to_string(s_sites.size()) + " unique callers ---");
        for (auto& [rva, site] : s_sites)
        {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "  RVA=0x%llX count=%llu",
                (unsigned long long)rva,
                (unsigned long long)site.count);
            WriteLog(buf);
        }
    }

    return result;
}

namespace DeltaTimeTrace
{
    void Init()
    {
        s_gameBase = (uintptr_t)GetModuleHandle(nullptr);
        uintptr_t addr = s_gameBase + kDeltaTimeRVA;

        std::cout << "[DT] Time.get_deltaTime @ 0x" << std::hex << addr
                  << " (RVA 0x" << kDeltaTimeRVA << ")" << std::dec << std::endl;

        if (MH_CreateHook((void*)addr, (void*)hk_TimeGetDeltaTime,
                          (void**)&o_TimeGetDeltaTime) == MH_OK)
        {
            MH_EnableHook((void*)addr);
            s_enabled = true;
            std::cout << "[DT] Hook enabled. Logging to DeltaTimeTrace.log" << std::endl;
            WriteLog("=== DeltaTimeTrace started ===");
        }
        else
        {
            std::cout << "[DT] [ERR] MH_CreateHook failed" << std::endl;
        }
    }

    void Poll()
    {
        // 空操作：hook 内部已自节流
    }
}
