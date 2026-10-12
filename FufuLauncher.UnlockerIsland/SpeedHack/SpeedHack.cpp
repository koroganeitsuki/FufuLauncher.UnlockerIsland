/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "SpeedHack.h"
#include "../Config/Config.h"
#include "../Core/SharedState.h"
#include "../MinHook/MinHook.h"
#include "../Visual/Visual.h"
#include <iostream>

// 原始函数指针 (存储在 SharedState 中通过 extern 访问)
extern tQueryPerformanceCounter o_QueryPerformanceCounter;
extern tGetTickCount64 o_GetTickCount64;

// 内部状态变量
static LARGE_INTEGER g_LastRealTimeQPC = { 0 };

// 线程局部重入保护：防止 hook 内部调用 QPC 时无限递归
static thread_local bool g_InHook = false;

namespace SpeedHack {

    BOOL WINAPI hk_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
        if (!lpPerformanceCount) return FALSE;

        // 重入保护：如果当前线程已在 hook 内部，直接走 trampoline 避免递归
        if (g_InHook) {
            if (o_QueryPerformanceCounter) return o_QueryPerformanceCounter(lpPerformanceCount);
            lpPerformanceCount->QuadPart = 0;
            return TRUE;
        }

        g_InHook = true;

        if (!o_QueryPerformanceCounter || !o_QueryPerformanceCounter(&g_LastRealTimeQPC)) {
            g_InHook = false;
            lpPerformanceCount->QuadPart = 0;
            return TRUE;
        }

        static LARGE_INTEGER s_LastReal = { 0 };
        static LARGE_INTEGER s_LastFake = { 0 };
        static double s_FractionalRemainder = 0.0;

        if (s_LastReal.QuadPart == 0) {
            s_LastReal = g_LastRealTimeQPC;
            s_LastFake = g_LastRealTimeQPC;
        }

        if (Config::Get().enable_speedhack) {
            double exactDelta = (g_LastRealTimeQPC.QuadPart - s_LastReal.QuadPart) * Config::Get().game_speed;
            exactDelta += s_FractionalRemainder;
            LONGLONG intPart = (LONGLONG)exactDelta;
            s_FractionalRemainder = exactDelta - intPart;
            s_LastFake.QuadPart += intPart;
        } else {
            s_LastFake.QuadPart += (g_LastRealTimeQPC.QuadPart - s_LastReal.QuadPart);
        }

        s_LastReal = g_LastRealTimeQPC;
        lpPerformanceCount->QuadPart = s_LastFake.QuadPart;

        g_InHook = false;
        return TRUE;
    }

    ULONGLONG WINAPI hk_GetTickCount64() {
        return o_GetTickCount64();
        // 重入保护
        if (g_InHook) {
            if (o_GetTickCount64) return o_GetTickCount64();
            return 0;
        }

        g_InHook = true;

        if (!o_GetTickCount64) {
            g_InHook = false;
            return 0;
        }

        ULONGLONG current_real = o_GetTickCount64();

        static ULONGLONG s_LastRealTick = 0;
        static ULONGLONG s_LastFakeTick = 0;
        static ULONGLONG s_LastFakeTick100 = 0;

        if (s_LastRealTick == 0) {
            s_LastRealTick = current_real;
            s_LastFakeTick = current_real;
            s_LastFakeTick100 = current_real * 100;
        }

        if (Config::Get().enable_speedhack) {
            s_LastFakeTick100 += (ULONGLONG)((current_real - s_LastRealTick) * (ULONGLONG)(100 * Config::Get().game_speed + 0.5));
            s_LastFakeTick = (ULONGLONG)(s_LastFakeTick100 / 100);
        } else {
            s_LastFakeTick100 += (ULONGLONG)((current_real - s_LastRealTick) * 100);
            s_LastFakeTick = (ULONGLONG)(s_LastFakeTick100 / 100);
        }

        s_LastRealTick = current_real;

        g_InHook = false;
        return s_LastFakeTick;
    }

    void ResetState() {
        g_LastRealTimeQPC.QuadPart = 0;
    }

    bool Init() {
        // 仅在配置启用时才安装 hook，避免无谓的性能开销和潜在的递归/死锁
        if (!Config::Get().enable_speedhack) {
            std::cout << "[SCAN] [SpeedHack] Disabled by config, skipping hook installation." << std::endl;
            return true;
        }

        // 现代 Windows 上 kernel32!QueryPerformanceCounter 是到 kernelbase.dll 的转发存根
        HMODULE hKernelBase = GetModuleHandleA("kernelbase.dll");
        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");

        // --- QueryPerformanceCounter ---
        void* addrQPC = nullptr;
        if (hKernelBase) addrQPC = (void*)GetProcAddress(hKernelBase, "QueryPerformanceCounter");
        if (!addrQPC && hKernel32) addrQPC = (void*)GetProcAddress(hKernel32, "QueryPerformanceCounter");

        if (addrQPC) {
            std::cout << "[SCAN] [SpeedHack] QueryPerformanceCounter..." << std::endl;
            LogOffset("QueryPerformanceCounter", addrQPC, addrQPC);
            std::cout << "   -> Found at: " << addrQPC << std::endl;

            if (MH_CreateHook(addrQPC, &hk_QueryPerformanceCounter, (LPVOID*)&o_QueryPerformanceCounter) == MH_OK) {
                std::cout << "   -> Hook Ready." << std::endl;
            } else {
                std::cout << "   -> [ERR] MH_CreateHook QueryPerformanceCounter Failed." << std::endl;
                return false;
            }
        } else {
            std::cout << "[ERR] [SpeedHack] QueryPerformanceCounter not found." << std::endl;
            return false;
        }

        // --- GetTickCount64 ---
        void* addrGTC = nullptr;
        if (hKernelBase) addrGTC = (void*)GetProcAddress(hKernelBase, "GetTickCount64");
        if (!addrGTC && hKernel32) addrGTC = (void*)GetProcAddress(hKernel32, "GetTickCount64");

        if (addrGTC) {
            std::cout << "[SCAN] [SpeedHack] GetTickCount64..." << std::endl;
            LogOffset("GetTickCount64", addrGTC, addrGTC);
            std::cout << "   -> Found at: " << addrGTC << std::endl;

            if (MH_CreateHook(addrGTC, &hk_GetTickCount64, (LPVOID*)&o_GetTickCount64) == MH_OK) {
                std::cout << "   -> Hook Ready." << std::endl;
            } else {
                std::cout << "   -> [WARN] MH_CreateHook GetTickCount64 Failed." << std::endl;
            }
        } else {
            std::cout << "[WARN] [SpeedHack] GetTickCount64 not found." << std::endl;
        }

        return true;
    }
}
