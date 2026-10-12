/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace SpeedHack {
    // 安装时间控制钩子 (QueryPerformanceCounter & GetTickCount64)
    bool Init();

    // 钩子函数 - 性能计数器 (用于游戏速度控制)
    BOOL WINAPI hk_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);

    // 钩子函数 - 系统滴答计数 (用于游戏速度控制)
    ULONGLONG WINAPI hk_GetTickCount64();

    // 重置内部时间状态 (例如场景切换后调用)
    void ResetState();
}
