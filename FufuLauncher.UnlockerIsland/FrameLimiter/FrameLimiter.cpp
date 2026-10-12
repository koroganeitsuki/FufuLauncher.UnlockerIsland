/*
Copyright (c) FufuLauncher Dev Team. All rights reserved.
Licensed under the AGPL-3.0 License.
*/
#include "FrameLimiter.h"
#include "../Config/Config.h"
#include <iostream>

static LARGE_INTEGER s_QPF = { 0 };      // QPC 频率 (ticks/s)
static LONGLONG      s_LastEnter = 0;     // 上次进入 hk_GameUpdate 的 QPC ticks

namespace FrameLimiter
{
    void Init()
    {
        QueryPerformanceFrequency(&s_QPF);
        std::cout << "[Limiter] Ready. QPF=" << s_QPF.QuadPart
                  << " ticks/sec. Interval="
                  << Config::Get().target_frame_interval_ms << " ms." << '\n';
    }

    // 每帧入口调用：按档位规则阻塞直到目标帧间隔
    void WaitForTarget()
    {
        const auto& cfg = Config::Get();
        if (!cfg.enable_frame_limiter || s_QPF.QuadPart == 0) return;

        LARGE_INTEGER nowQpc{};
        QueryPerformanceCounter(&nowQpc);

        // 首帧不阻塞
        if (s_LastEnter == 0)
        {
            s_LastEnter = nowQpc.QuadPart;
            return;
        }

        // 把 ms 目标间隔换成 ticks
        double intervalMs = cfg.target_frame_interval_ms;
        LONGLONG intervalTicks = (LONGLONG)(intervalMs * (double)s_QPF.QuadPart / 1000.0);

        LONGLONG elapsed = nowQpc.QuadPart - s_LastEnter;

        if (elapsed < intervalTicks)
        {
            // 档位逻辑：当前帧耗时 < interval → 补等到 interval
            LONGLONG remaining = intervalTicks - elapsed;

            // Sleep 部分：先睡掉大部分剩余时间（系统定时器精度 ≈ 15.6ms
            // 但 Sleep(1) 在 Win10+ 使用 NtDelayExecution 可以做到 ~1ms 精度）
            LONGLONG sleepMs = (LONGLONG)((double)remaining * 1000.0 / (double)s_QPF.QuadPart);
            if (sleepMs > 1) Sleep((DWORD)(sleepMs - 1));

            // 自旋部分：精确等到 interval 到达（避免 Sleep 超时）
            while (true)
            {
                LARGE_INTEGER spinQpc{};
                QueryPerformanceCounter(&spinQpc);
                if (spinQpc.QuadPart - s_LastEnter >= intervalTicks) break;
            }
        }
        // elapsed >= intervalTicks → 上一帧已经超时，直接放行（档位自然后移）

        s_LastEnter = nowQpc.QuadPart;
    }
}
